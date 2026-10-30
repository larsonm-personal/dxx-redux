#include "named_audio.h"
#include "named_audio_state.h"
#include "classic_audio.h"
#include "shared_audio.h"
#include "shared_audio_internal.h"
#include "linked_audio.h"
#include "linked_audio_state.h"
#include "gameplay_audio_state.h"
#include "seismic_audio_state.h"
#include "audio_resource.h"
#include "preview.h"
#include "doorway.h"
#include "../audio_voice.h"
#include "../ambient_sound.h"
#include "../named_sound.h"
#include "../inventory.h"
#include "../level_text.h"
#include "../capacity.h"
#include "picosha2.h"
#include "save_epoch_format.h"
#include <SDL.h>
#ifdef USE_SDLMIXER
#include <SDL_mixer.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#ifdef __ANDROID__
#include "music_playback_levels.h"
#endif
extern "C" {
#include "inferno.h"
#include "physfsx.h"
#include "mission.h"
#include "switch.h"
#include "game.h"
#include "wall.h"
#include "digi.h"
#include "args.h"
#include "console.h"
}

namespace xl_compat
{
struct AudioResources {
	struct Action {
		int id, repetitions;
		unsigned sample;
	};
	std::string identity;
	std::vector<WaveChannels> sources;
	std::vector<std::string> names;
	std::vector<unsigned> classic;
	std::vector<std::string> classic_names;
	std::map<std::string, std::string> classic_replacements;
	std::map<int, Action> actions;
	std::map<std::string, unsigned> external;
	unsigned prepared_rate = 0;
	std::shared_ptr<const std::vector<AudioSample>> prepared_samples;
};
} // namespace xl_compat

namespace xl_compat::audio_runtime
{
AudioVoices mixer;
AudioOwners owners;
bool backend_ready = false;
int priority_volume = 32768;
std::string failure;
AudioLock::AudioLock() : locked(backend_ready)
{
	if (locked) SDL_LockAudio();
}
AudioLock::~AudioLock()
{
	if (locked) SDL_UnlockAudio();
}
} // namespace xl_compat::audio_runtime

namespace
{
using namespace xl_compat;
using namespace xl_compat::audio_runtime;
constexpr unsigned event_limit = DXX_XL_MAX_TRIGGERS * 2;
struct AudioEvent {
	std::int64_t started = 0, cooldown = -1;
	std::uint64_t serial = 0;
	unsigned rate = 0;
	bool playing = false, activated = false;
};
std::shared_ptr<const AudioResources> bank;
std::shared_ptr<const std::vector<AudioSample>> converted;
std::array<AudioEvent, event_limit> events;
bool resume_pending = false;
unsigned output_rate = 44100;
std::uint64_t callbacks = 0, output_frames = 0, failed_starts = 0;

std::string external_key(const std::string &filename)
{
	auto key = named_sound_filename(filename);
	SDL_strlwr(key.data());
	return key;
}

std::string owned_bytes(std::string name, std::size_t limit)
{
	if (PHYSFSEXT_locateCorrectCase(name.data()) != 0)
		throw std::runtime_error("Missing XL named-audio resource in the mission HOG: " + name);
	if (!mission_owns_resource_archive(PHYSFS_getRealDir(name.c_str())))
		throw std::runtime_error("XL named-audio resource is shadowed outside the selected mission HOG: " + name);
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length < 0 || std::uint64_t(length) > limit)
		throw std::runtime_error("Invalid bounded XL named-audio resource: " + name);
	std::string bytes(std::size_t(length), '\0');
	if (PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) != length)
		throw std::runtime_error("Truncated XL named-audio resource: " + name);
	return bytes;
}

std::int64_t duration(const AudioSample &sample)
{
	// CAudioChannel::Duration uses converted stereo bytes and a half-second tail
	const float seconds = float(sample.stereo.size() * sizeof(std::int16_t)) / float(sample.rate * 4u) + 0.5f;
	if (seconds * 65536.0f >= float(INT32_MAX)) throw std::runtime_error("Named-audio duration exceeds the source clock range");
	return std::int32_t(seconds * 65536.0f);
}

std::vector<AudioSample> convert_bank(const AudioResources &resources, unsigned rate)
{
	std::vector<AudioSample> result;
	std::size_t bytes = 0;
	for (const auto &source : resources.sources) {
		auto sample = prepare_audio_sample(source, rate);
		bytes += sample.stereo.size() * sizeof(std::int16_t);
		if (bytes > 128 * 1024 * 1024) throw std::runtime_error("Named-audio bank exceeds 128 MiB of mixer storage");
		result.push_back(std::move(sample));
	}
	for (const auto &[key, action] : resources.actions) {
		(void) key;
		if (action.repetitions > 0 && duration(result.at(action.sample)) * action.repetitions > INT32_MAX)
			throw std::runtime_error("Named-audio repetition cooldown exceeds the source clock range");
	}
	return result;
}

#ifdef USE_SDLMIXER
void mix_output(int, void *stream, int length, void *)
{
	if (!backend_ready || !bank || !converted || length <= 0 || length % 4) return;
	mixer.mix(static_cast<unsigned char *>(stream), std::size_t(length) / 4, *converted);
	++callbacks;
	output_frames += unsigned(length) / 4;
}
#endif
} // namespace

int xl_compat::audio_runtime::classic_sample(int physical_sample)
{
	if (!xl_level_extensions_active() || !bank || physical_sample < 0 || unsigned(physical_sample) >= bank->classic.size()) return -1;
	return int(bank->classic[unsigned(physical_sample)]);
}

int xl_compat::audio_runtime::external_sample(const char *filename)
{
	if (!xl_level_extensions_active() || !bank || !filename || !*filename) return -1;
	const auto key = external_key(filename);
	const auto found = bank->external.find(key);
	if (found == bank->external.end()) throw std::runtime_error("XL external sound was not prepared from the selected mission: " + key);
	return int(found->second);
}

bool xl_compat::audio_runtime::external_sample_valid(unsigned sample)
{
	if (!xl_level_extensions_active() || !bank) return false;
	return std::any_of(bank->external.begin(), bank->external.end(), [&](const auto &entry) { return entry.second == sample; });
}

namespace xl_compat
{
static std::shared_ptr<AudioResources> prepare_named_only(const char *level, const Inventory &inventory, int ordinal)
{
	auto resources = std::make_shared<AudioResources>();
	const auto add = [&](const Trigger &action, int key) {
		if (action.type != 30) return;
		if (key < 0 || unsigned(key) >= event_limit || action.value / 65536 <= 0)
			throw std::runtime_error("XL named sound requires a positive SND ID and a bounded action owner");
		resources->actions.emplace(key, AudioResources::Action{ action.value / 65536, action.time, 0 });
	};
	for (unsigned i = 0; i < inventory.triggers.size(); ++i) add(inventory.triggers[i], int(i));
	for (unsigned i = 0; i < inventory.object_trigger_records.size(); ++i) add(inventory.object_trigger_records[i].action, DXX_XL_MAX_TRIGGERS + int(i));
	if (resources->actions.empty()) return {};
#ifndef USE_SDLMIXER
	throw std::runtime_error("XL named sounds require the SDL_mixer backend");
#endif
	if (!GameArg.SndNoSound && !backend_ready)
		throw std::runtime_error("XL named sounds require an initialized stereo 16-bit SDL_mixer output");
	std::string name = level ? level : "";
	if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name.find("..") != std::string::npos)
		throw std::runtime_error("Invalid XL named-audio level identity");
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos) throw std::runtime_error("XL named-audio level has no filename extension");
	name.resize(dot);
	name += ".snd";
	const auto text = owned_bytes(name, 1024 * 1024);
	const auto table = decode_level_text(text);
	std::string identity = picosha2::hash256_hex_string(text);
	std::size_t bytes = 0;
	for (auto &[key, action] : resources->actions) {
		(void) key;
		const auto *row = find_level_text(table, action.id);
		if (!row) throw std::runtime_error("XL sound ID " + std::to_string(action.id) + " is missing from " + name);
		const auto filename = external_key(row->text);
		const auto previous = resources->external.find(filename);
		if (previous != resources->external.end()) {
			action.sample = previous->second;
			continue;
		}
		const auto resource = load_audio_resource(row->text, level, ordinal);
		const auto &wav = resource.bytes;
		WaveChannels pcm;
		try {
			pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(wav.data()), wav.size());
		} catch (const std::exception &error) {
			throw std::runtime_error("Invalid XL named sound " + resource.path + ": " + error.what());
		}
		bytes += pcm.samples.size() * sizeof(std::int16_t);
		if (bytes > 128 * 1024 * 1024) throw std::runtime_error("Named-audio bank exceeds 128 MiB of source storage");
		action.sample = unsigned(resources->sources.size());
		resources->external.emplace(filename, action.sample);
		resources->sources.push_back(std::move(pcm));
		resources->names.push_back(resource.path);
		identity += ":" + resource.path + ":" + picosha2::hash256_hex_string(wav);
	}
	resources->identity = picosha2::hash256_hex_string(identity);
	return resources;
}

std::shared_ptr<const AudioResources> prepare_named_audio(const char *level, const Inventory &inventory, int ordinal)
{
	auto resources = prepare_named_only(level, inventory, ordinal);
	if (!resources) resources = std::make_shared<AudioResources>();
	std::size_t bytes = 0;
	for (const auto &pcm : resources->sources) bytes += pcm.samples.size() * sizeof(std::int16_t);
	std::string external_identity;
	for (const auto &object : inventory.objects) {
		if (object.render != 13) continue;
#ifndef USE_SDLMIXER
		throw std::runtime_error("XL ambient sounds require the SDL_mixer backend");
#endif
		if (!GameArg.SndNoSound && !backend_ready) throw std::runtime_error("XL ambient sounds require an initialized stereo 16-bit SDL_mixer output");
		const auto sound = decode_ambient_sound(object);
		if (sound.volume < 0) throw std::runtime_error("XL ambient sound object has a negative playback volume");
		auto name = sound.filename;
		if (name.empty()) throw std::runtime_error("XL ambient sound object has an empty filename");
		name += ".wav";
		const auto key = external_key(name);
		if (resources->external.count(key)) continue;
		const auto resource = load_audio_resource(name, level, ordinal);
		WaveChannels pcm;
		try {
			pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource.bytes.data()), resource.bytes.size());
		} catch (const std::exception &error) {
			throw std::runtime_error("Invalid XL ambient sound " + resource.path + ": " + error.what());
		}
		bytes += pcm.samples.size() * sizeof(std::int16_t);
		if (bytes > 128 * 1024 * 1024) throw std::runtime_error("XL shared bank exceeds 128 MiB of source storage");
		resources->external.emplace(key, unsigned(resources->sources.size()));
		resources->sources.push_back(std::move(pcm));
		resources->names.push_back(resource.path);
		external_identity += ":" + name + ":" + resource.path + ":" + picosha2::hash256_hex_string(resource.bytes);
	}
	const auto replacements = prepare_classic_audio(level, ordinal, classic_audio_output_rate());
	for (const auto &sample : replacements.samples) resources->classic_replacements.emplace(sample.name, sample.path);
	if (!replacements.samples.empty()) {
#ifndef USE_SDLMIXER
		throw std::runtime_error("XL classic replacements require the SDL_mixer backend");
#endif
		if (!GameArg.SndNoSound && !backend_ready) throw std::runtime_error("XL classic replacements require an initialized stereo 16-bit SDL_mixer output");
	}
	auto classic = prepare_classic_playback(replacements);
	for (auto &sample : classic.samples) {
		if (sample.sample != int(resources->classic.size())) throw std::runtime_error("XL shared bank has an incomplete classic registry");
		bytes += sample.pcm.samples.size() * sizeof(std::int16_t);
		if (bytes > 128 * 1024 * 1024) throw std::runtime_error("XL shared bank exceeds 128 MiB of source storage");
		resources->classic.push_back(unsigned(resources->sources.size()));
		resources->classic_names.push_back(sample.name);
		resources->sources.push_back(std::move(sample.pcm));
	}
	resources->identity = picosha2::hash256_hex_string("xl-shared-audio-v2:" + resources->identity + external_identity + ":" + classic.identity);
	resources->prepared_rate = output_rate;
	// Reuse only immutable PCM. Authored actions and resource validation above
	// remain fresh even when a different level selects the same sound content
	resources->prepared_samples = bank && bank->identity == resources->identity && bank->prepared_rate == output_rate
	                                  ? bank->prepared_samples
	                                  : std::make_shared<const std::vector<AudioSample>>(convert_bank(*resources, output_rate));
	return resources;
}

void install_named_audio(std::shared_ptr<const AudioResources> resources)
{
	// Preparation and installation run on the game thread without reopening audio
	if (resources && resources->prepared_rate != output_rate) throw std::runtime_error("XL named-audio output changed during level preparation");
	auto samples = resources ? resources->prepared_samples : nullptr;
	std::shared_ptr<const AudioResources> previous;
	{
		AudioLock lock;
		owners.clear(mixer);
		mixer.clear();
		looping = {};
		events.fill({});
		// Reset XL cadence on level/resource transitions without touching stock-only loads
		if (bank || resources) reset_seismic_audio();
		previous = std::move(bank);
		bank = std::move(resources);
		converted.swap(samples);
		failure.clear();
	}
	clear_linked_scene();
}
} // namespace xl_compat

extern "C" int xl_named_audio_backend_open(int rate, unsigned format, int channels)
{
#ifdef USE_SDLMIXER
	try {
		if (backend_ready) return rate == int(output_rate) && format == AUDIO_S16SYS && channels == 2;
		if (rate < 8000 || rate > 192000 || format != AUDIO_S16SYS || channels != 2) {
			failure = "Unsupported XL named-audio output: rate=" + std::to_string(rate) + ", format=" + std::to_string(format) + ", channels=" + std::to_string(channels);
			return 0;
		}
		auto samples = bank ? std::make_shared<const std::vector<AudioSample>>(convert_bank(*bank, unsigned(rate))) : nullptr;
		for (auto &voice : mixer.voices) {
			if (!voice.remaining) continue;
			voice.position = unsigned((std::uint64_t(voice.position) * unsigned(rate)) / output_rate);
			if (samples && voice.sample < samples->size()) voice.position = (std::min) (voice.position, unsigned(samples->at(voice.sample).frames() - 1));
		}
		converted = std::move(samples);
		output_rate = unsigned(rate);
		backend_ready = true;
		if (!Mix_RegisterEffect(MIX_CHANNEL_POST, mix_output, nullptr, nullptr)) {
			backend_ready = false;
			failure = "Cannot register XL named-audio output: " + std::string(Mix_GetError());
			return 0;
		}
		xl_named_audio_set_volume(digi_volume);
		return 1;
	} catch (const std::exception &error) {
		backend_ready = false;
		failure = error.what();
	}
#else
	(void) rate;
	(void) format;
	(void) channels;
#endif
	return 0;
}

extern "C" void xl_named_audio_backend_close(void)
{
#ifdef USE_SDLMIXER
	if (backend_ready) Mix_UnregisterEffect(MIX_CHANNEL_POST, mix_output);
#endif
	backend_ready = false;
}

extern "C" void xl_named_audio_set_volume(int volume)
{
	// Match the existing SFX slider's integer level and Android headroom
	// Source effects gain belongs to each channel; only platform headroom is global
	float gain = 128.0f;
#ifdef __ANDROID__
	gain *= AUDIO_EFFECTS_VOLUME_SCALE * AUDIO_GAMEPLAY_HEADROOM_SCALE;
#endif
	{
		AudioLock lock;
		mixer.volume = int(gain);
		priority_volume = (std::clamp) (volume, 0, 32768);
	}
	xl_audio_sync_objects();
}

extern "C" void xl_named_audio_pause(int paused)
{
	AudioLock lock;
	mixer.paused = paused != 0;
	resume_pending = false;
}
extern "C" void xl_named_audio_resume(void)
{
	// Restore can reactivate the window before gameplay or its pause overlay
	AudioLock lock;
	mixer.paused = true;
	resume_pending = true;
}
extern "C" void xl_named_audio_frame(void)
{
	if (game_is_time_paused()) return;
	if (resume_pending) {
		AudioLock lock;
		mixer.paused = false;
		resume_pending = false;
	}
	xl_audio_sync_objects();
}
extern "C" void xl_named_audio_stop(void)
{
	AudioLock lock;
	owners.clear(mixer);
	mixer.clear();
	looping = {};
}
extern "C" void xl_named_audio_clear(void)
{
	xl_compat::install_named_audio({});
}
extern "C" const char *xl_named_audio_error(void)
{
	return failure.c_str();
}

extern "C" int xl_audio_start_sample(int sample, int sound_class, int volume, int pan, int looping, int loop_start, int loop_end)
{
	const int mapped = classic_sample(sample);
	return mapped < 0 ? -1 : start_resource_sample(unsigned(mapped), sound_class, volume, pan, looping, loop_start, loop_end);
}
int xl_compat::audio_runtime::start_resource_sample(unsigned sample, int sound_class, int volume, int pan, int looping, int loop_start, int loop_end)
{
	try {
		if (!xl_level_extensions_active() || !backend_ready || !bank || sample >= bank->sources.size() || sound_class < 0 || sound_class > 6 || volume < 0 || pan < 0 || pan > 65536) return -1;
		const auto repetitions = looping ? -1 : std::int64_t(loop_end) - loop_start + 1;
		const auto effective = std::int64_t(volume) * priority_volume / 65536;
		if (repetitions < INT32_MIN || repetitions > INT32_MAX || effective > INT32_MAX / 2) return -1;
		AudioLock lock;
		owners.reconcile(mixer);
		return owners.start_voice(sample, int(repetitions), -1,
		                          audio_priority(sound_class, volume, int(effective), -1, looping != 0), audio_start_output(int(effective), pan), external_sample_valid(sample) ? AudioOwnerKind::external_unlinked : AudioOwnerKind::unlinked, mixer);
	} catch (const std::exception &error) {
		failure = error.what();
		return -1;
	}
}
extern "C" int xl_audio_channel_playing(int slot)
{
	if (!xl_level_extensions_active()) return 0;
	AudioLock lock;
	return slot >= 0 && unsigned(slot) < mixer.voices.size() && mixer.voices[unsigned(slot)].remaining != 0;
}
extern "C" void xl_audio_stop_channel(int slot)
{
	if (!xl_level_extensions_active()) return;
	AudioLock lock;
	mixer.stop(slot);
	owners.reconcile(mixer);
}
extern "C" int xl_audio_set_channel_volume(int slot, int volume)
{
	if (!xl_level_extensions_active()) return 0;
	AudioLock lock;
	return mixer.set_volume(slot, volume, priority_volume);
}
extern "C" int xl_audio_set_channel_pan(int slot, int pan)
{
	if (!xl_level_extensions_active() || pan < 0 || pan > 65536) return 0;
	AudioLock lock;
	return mixer.set_pan(slot, pan);
}

extern "C" int xl_named_audio_action(trigger *action, int owner)
{
	if (!xl_level_extensions_active() || !action || action->type != 30 || (action->flags & TF_DISABLED) || !bank) return 0;
	const auto found = bank->actions.find(owner);
	if (found == bank->actions.end() || action->value / 65536 != found->second.id || action->time != found->second.repetitions) return 0;
	auto &event = events[unsigned(owner)];
	if (!event.activated) {
		event.started = GameTime64;
		event.activated = true;
	}
	if (action->time >= 0 && GameTime64 - event.started <= event.cooldown) return 0;
	const auto &authored = found->second;
	AudioLock lock;
	const int slot = backend_ready ? owners.start_voice(authored.sample, authored.repetitions, owner,
	                                                    audio_priority(0, 65536, priority_volume, -1, authored.repetitions < 0), audio_start_output(priority_volume, 32767), AudioOwnerKind::action, mixer)
	                               : -1;
	if (slot < 0) {
		if (!GameArg.SndNoSound) {
			++failed_starts;
			failure = backend_ready ? "XL named-audio voice allocation failed" : "XL named-audio output is unavailable";
		}
		event.started = GameTime64;
		return 1;
	}
	event.serial = mixer.voices[unsigned(slot)].serial;
	event.playing = true;
	event.started = GameTime64;
	if (action->time >= 0) {
		event.rate = output_rate;
		event.cooldown = duration(converted->at(authored.sample)) * action->time;
	}
	return 1;
}

namespace xl_compat
{
namespace
{
std::int64_t integer(const nlohmann::json &value, std::int64_t minimum, std::int64_t maximum)
{
	if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<std::uint64_t>() > std::uint64_t(maximum)))
		throw std::runtime_error("Invalid named-audio state integer");
	const auto result = value.get<std::int64_t>();
	if (result < minimum || result > maximum) throw std::runtime_error("Named-audio state integer is out of range");
	return result;
}

std::uint64_t frames_at_rate(const WaveChannels &source, unsigned rate)
{
	return (std::max) (std::uint64_t(1), (std::uint64_t(source.samples.size() / source.source_channels) * rate + source.rate / 2) / source.rate);
}

std::int64_t source_duration(const WaveChannels &source, unsigned rate)
{
	const float seconds = float(frames_at_rate(source, rate) * 4) / float(rate * 4) + 0.5f;
	if (seconds * 65536.0f >= float(INT32_MAX)) throw std::runtime_error("Saved named-audio duration exceeds the source clock range");
	return std::int32_t(seconds * 65536.0f);
}
} // namespace

nlohmann::json named_audio_snapshot()
{
	// A removed native object can precede its next frame update. Save its stopped
	// lifetime, never an active channel bound to a different object signature
	xl_audio_destroy_object(-1, -1);
	std::array<AudioVoice, audio_voice_limit> voices;
	AudioOwners saved_owners;
	LoopingAudioState saved_loop;
	std::uint64_t next;
	unsigned cursor;
	{
		AudioLock lock;
		owners.reconcile(mixer);
		if (!looping.matches(mixer)) looping = {};
		saved_loop = looping;
		saved_owners = owners;
		voices = mixer.voices;
		next = mixer.next_serial;
		cursor = mixer.cursor;
	}
	nlohmann::json state = { { "source", bank ? bank->identity : "" }, { "rate", output_rate }, { "next", next }, { "cursor", cursor }, { "events", nlohmann::json::array() }, { "voices", nlohmann::json::array() } };
	state["linked"] = encode_linked_audio(saved_owners, deferred);
	state["loop"] = saved_loop.channel < 0 ? nlohmann::json::array() : nlohmann::json{ saved_loop.channel, saved_loop.serial };
	const auto seismic = bank ? seismic_audio_snapshot() : SeismicAudioState{ GameTime64, 0 };
	state["seismic"] = { seismic.previous_volume ? seismic.next_volume_time - GameTime64 : 0, seismic.previous_volume };
	if (bank)
		for (const auto &[key, action] : bank->actions) {
			(void) action;
			const auto &event = events[unsigned(key)];
			const auto elapsed = !event.activated ? -1 : (std::max) (std::int64_t(0), GameTime64 - event.started);
			state["events"].push_back({ key, elapsed, event.cooldown, event.serial, event.rate, event.playing });
		}
	for (unsigned slot = 0; slot < voices.size(); ++slot) {
		const auto &voice = voices[slot];
		if (voice.remaining) state["voices"].push_back({ slot, voice.serial, voice.sample, voice.position, voice.owner, voice.remaining, voice.priority.volume, int(voice.kind), voice.priority.sound_class, voice.priority.persistent, voice.output.volume, voice.output.pan[0], voice.output.pan[1] });
	}
	return state;
}

bool restore_named_audio(const nlohmann::json &state, bool apply)
{
	try {
		if (!state.is_object() || state.size() != 9 || !state.at("source").is_string() ||
		    state.at("source").get<std::string>() != (bank ? bank->identity : "") ||
		    !state.at("events").is_array() || state.at("events").size() != (bank ? bank->actions.size() : 0) ||
		    !state.at("voices").is_array() || state.at("voices").size() > audio_voice_limit)
			throw std::runtime_error("Named-audio save has different resources or layout");
		const auto rate = unsigned(integer(state.at("rate"), 8000, 192000));
		const auto next = std::uint64_t(integer(state.at("next"), 1, INT64_MAX));
		const auto cursor = unsigned(integer(state.at("cursor"), 0, audio_voice_limit - 1));
		std::array<AudioEvent, event_limit> restored_events;
		std::set<unsigned> action_owners, slots;
		std::set<std::uint64_t> serials, event_serials;
		for (const auto &row : state.at("events")) {
			if (!row.is_array() || row.size() != 6 || !row[5].is_boolean()) throw std::runtime_error("Invalid named-audio event row");
			const auto owner = unsigned(integer(row[0], 0, event_limit - 1));
			if (!bank || !bank->actions.count(int(owner)) || !action_owners.insert(owner).second) throw std::runtime_error("Unknown or duplicate named-audio action owner");
			const auto &authored = bank->actions.at(int(owner));
			auto &event = restored_events[owner];
			const auto elapsed = integer(row[1], -1, SAVE_EPOCH_MAX);
			event.activated = elapsed >= 0;
			event.started = elapsed < 0 ? 0 : GameTime64 - elapsed;
			event.cooldown = integer(row[2], -1, INT32_MAX);
			event.serial = std::uint64_t(integer(row[3], 0, std::int64_t(next - 1)));
			if (event.serial && !event_serials.insert(event.serial).second) throw std::runtime_error("Duplicate named-audio event serial");
			event.rate = unsigned(integer(row[4], 0, 192000));
			event.playing = row[5].get<bool>();
			if (elapsed < 0 && (event.cooldown != -1 || event.serial || event.rate || event.playing)) throw std::runtime_error("Unstarted named-audio action has live state");
			if (event.playing && !event.serial) throw std::runtime_error("Playing named-audio action has no voice identity");
			if (event.rate) {
				if (event.rate < 8000 || authored.repetitions < 0 || !event.serial ||
				    event.cooldown != source_duration(bank->sources.at(authored.sample), event.rate) * authored.repetitions)
					throw std::runtime_error("Named-audio cooldown differs from the authored repetitions");
			} else if (event.cooldown != -1 || (event.serial && authored.repetitions >= 0)) throw std::runtime_error("Invalid named-audio cooldown format");
		}
		std::array<AudioVoice, audio_voice_limit> voices{};
		for (const auto &row : state.at("voices")) {
			if (!row.is_array() || row.size() != 13 || !bank || !row[9].is_boolean()) throw std::runtime_error("Invalid named-audio voice row");
			const auto slot = unsigned(integer(row[0], 0, audio_voice_limit - 1));
			if (!slots.insert(slot).second) throw std::runtime_error("Duplicate named-audio voice slot");
			auto &voice = voices[slot];
			voice.serial = std::uint64_t(integer(row[1], 1, std::int64_t(next - 1)));
			voice.sample = unsigned(integer(row[2], 0, std::int64_t(bank->sources.size() - 1)));
			voice.owner = int(integer(row[4], -1, event_limit - 1));
			voice.remaining = int(integer(row[5], -1, INT32_MAX));
			voice.kind = static_cast<AudioOwnerKind>(integer(row[7], 0, 3));
			voice.priority = { int(integer(row[8], -1, 6)), int(integer(row[6], 0, INT32_MAX / 2)), row[9].get<bool>() };
			voice.output = { int(integer(row[10], 0, 128)), { std::uint8_t(integer(row[11], 0, 255)), std::uint8_t(integer(row[12], 0, 255)) } };
			if (!voice.remaining || !serials.insert(voice.serial).second || voice.output.volume != audio_start_output(voice.priority.volume, 32767).volume ||
			    (unsigned(voice.output.pan[0]) + voice.output.pan[1] != 254 && voice.output.pan != std::array<std::uint8_t, 2>{ 255, 255 }))
				throw std::runtime_error("Invalid shared-audio channel state");
			if (voice.kind == AudioOwnerKind::action) {
				if (!bank->actions.count(voice.owner)) throw std::runtime_error("Invalid named-audio voice identity");
				const auto &action = bank->actions.at(voice.owner);
				const auto expected = audio_priority(0, 65536, voice.priority.volume, -1, action.repetitions < 0);
				const auto &event = restored_events[unsigned(voice.owner)];
				if (voice.priority.sound_class != expected.sound_class || voice.priority.persistent != expected.persistent ||
				    action.sample != voice.sample || !event.activated || event.serial < voice.serial ||
				    (action.repetitions > 0 ? (voice.remaining < 1 || voice.remaining > action.repetitions) : voice.remaining != -1))
					throw std::runtime_error("Named-audio voice differs from its authored action");
			} else if (voice.kind == AudioOwnerKind::unlinked || voice.kind == AudioOwnerKind::external_unlinked) {
				const bool valid_sample = voice.kind == AudioOwnerKind::external_unlinked ? external_sample_valid(voice.sample)
				                                                                          : std::find(bank->classic.begin(), bank->classic.end(), voice.sample) != bank->classic.end();
				if (voice.owner != -1 || !valid_sample ||
				    (voice.priority.sound_class == 0 && voice.priority.persistent) || (voice.priority.sound_class == -1 && !voice.priority.persistent))
					throw std::runtime_error("Unlinked audio voice has an invalid sample, owner or class");
			}
			const auto frame_count = frames_at_rate(bank->sources.at(voice.sample), rate);
			const auto position = std::uint64_t(integer(row[3], 0, std::int64_t(frame_count - 1)));
			voice.position = unsigned((std::min) (position * output_rate / rate, std::uint64_t(converted->at(voice.sample).frames() - 1)));
		}
		AudioVoices saved_mixer;
		saved_mixer.voices = voices;
		saved_mixer.next_serial = next;
		saved_mixer.cursor = cursor;
		const auto linked = decode_linked_audio(state.at("linked"), saved_mixer);
		LoopingAudioState saved_loop;
		const auto &seismic = state.at("seismic");
		if (!seismic.is_array() || seismic.size() != 2) throw std::runtime_error("Invalid seismic audio timing layout");
		const SeismicAudioState saved_seismic{ GameTime64 + integer(seismic[0], -SAVE_EPOCH_MAX, SAVE_EPOCH_MAX), int(integer(seismic[1], 0, 80)) };
		// Four detonation slots plus one level disturbance, each contributing <= 16
		if (!save_timestamp_valid(saved_seismic.next_volume_time) || (!saved_seismic.previous_volume && seismic[0] != 0) || (!bank && saved_seismic.previous_volume)) throw std::runtime_error("Invalid seismic audio deadline");
		const auto &loop = state.at("loop");
		if (!loop.is_array() || (!loop.empty() && loop.size() != 2)) throw std::runtime_error("Invalid looping-audio binding");
		if (!loop.empty()) {
			saved_loop.channel = int(integer(loop[0], 0, audio_voice_limit - 1));
			saved_loop.serial = std::uint64_t(integer(loop[1], 1, std::int64_t(next - 1)));
			if (!saved_loop.matches(saved_mixer)) throw std::runtime_error("Looping-audio binding differs from its live channel");
		}
		if (apply) {
			AudioLock lock;
			// Do not consume restored samples while the loader is still running
			resume_pending = resume_pending || !mixer.paused;
			mixer.paused = true;
			owners.clear(mixer);
			owners = linked.owners;
			deferred = linked.deferred;
			looping = saved_loop;
			if (bank) restore_seismic_audio(saved_seismic);
			mixer.voices = voices;
			mixer.next_serial = next;
			mixer.cursor = cursor;
			events = restored_events;
			failure.clear();
		}
		return true;
	} catch (const std::exception &error) {
		failure = error.what();
		return false;
	}
}

nlohmann::json named_audio_diagnostics()
{
	AudioVoices copy;
	std::uint64_t delivered, calls;
	{
		AudioLock lock;
		copy = mixer;
		delivered = output_frames;
		calls = callbacks;
	}
	nlohmann::json result = { { "backend_ready", backend_ready }, { "rate", output_rate }, { "resources", bank ? bank->sources.size() : 0 }, { "actions", bank ? bank->actions.size() : 0 }, { "files", bank ? bank->names : std::vector<std::string>{} }, { "paused", copy.paused }, { "resume_pending", resume_pending }, { "effects_volume", priority_volume }, { "output_volume", copy.volume }, { "classic_samples", bank ? bank->classic.size() : 0 }, { "starts", copy.starts }, { "completions", copy.completions }, { "stops", copy.stops }, { "failed_starts", failed_starts }, { "output_callbacks", calls }, { "output_frames", delivered }, { "error", failure }, { "voices", nlohmann::json::array() } };
	result["cursor"] = copy.cursor;
	result["classic_replacements"] = bank ? nlohmann::json(bank->classic_replacements) : nlohmann::json::object();
	std::set<std::string> named_files;
	result["external_resources"] = nlohmann::json::object();
	if (bank) {
		for (const auto &[key, action] : bank->actions) {
			(void) key;
			named_files.insert(bank->names.at(action.sample));
		}
		for (const auto &[key, sample] : bank->external)
			result["external_resources"][key] = { { "sample", sample }, { "path", bank->names.at(sample) } };
	}
	result["named_files"] = named_files;
	for (const auto &voice : copy.voices)
		if (voice.remaining) result["voices"].push_back({ { "serial", voice.serial }, { "owner", voice.owner }, { "sample", voice.sample }, { "position", voice.position }, { "remaining", voice.remaining } });
	return result;
}
} // namespace xl_compat

extern "C" void xl_named_audio_wall_frame(int owner)
{
	if (!xl_level_extensions_active() || !bank || owner < 0 || owner >= Num_triggers || unsigned(owner) >= DXX_XL_MAX_TRIGGERS) return;
	auto &action = Triggers[owner];
	auto &event = events[unsigned(owner)];
	if (action.type != 30 || !event.playing || event.cooldown <= 0 || GameTime64 - event.started <= event.cooldown) return;
	event.playing = false;
	int wall = 0;
	for (int i = 0; i < Num_walls; ++i)
		if (wall_trigger_index(Walls[i].trigger) == owner) wall = i;
	const int passage = wall < Num_walls && Walls[wall].type != WALL_DOOR ? xl_wall_doorway(-1, Walls[wall].segnum, Walls[wall].sidenum) : WID_TRANSPARENT_WALL;
	const bool fly_through = (passage & (WID_TRANSPARENT_WALL | WID_WALL)) == 0;
	if (fly_through ? !(action.flags & TF_ONE_SHOT) : (action.flags & 8)) action.flags |= TF_DISABLED;
}
