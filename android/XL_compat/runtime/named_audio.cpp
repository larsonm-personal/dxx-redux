#include "named_audio.h"
#include "named_audio_state.h"
#include "preview.h"
#include "doorway.h"
#include "../audio_voice.h"
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
struct NamedAudioResources {
	struct Action {
		int id, repetitions;
		unsigned sample;
	};
	std::string identity;
	std::vector<WaveChannels> sources;
	std::vector<std::string> names;
	std::map<int, Action> actions;
	unsigned prepared_rate = 0;
	std::shared_ptr<const std::vector<AudioSample>> prepared_samples;
};
} // namespace xl_compat

namespace
{
using namespace xl_compat;
constexpr unsigned event_limit = DXX_XL_MAX_TRIGGERS * 2;
struct AudioEvent {
	std::int64_t started = 0, cooldown = -1;
	std::uint64_t serial = 0;
	unsigned rate = 0;
	bool playing = false, activated = false;
};
std::shared_ptr<const NamedAudioResources> bank;
std::shared_ptr<const std::vector<AudioSample>> converted;
std::array<AudioEvent, event_limit> events;
AudioVoices mixer;
bool backend_ready = false;
bool resume_pending = false;
unsigned output_rate = 44100;
std::string failure;
std::uint64_t callbacks = 0, output_frames = 0, failed_starts = 0;

struct AudioLock {
	bool locked = backend_ready;
	AudioLock()
	{
		if (locked) SDL_LockAudio();
	}
	~AudioLock()
	{
		if (locked) SDL_UnlockAudio();
	}
};

std::string owned_bytes(std::string name, std::size_t limit)
{
	if (PHYSFSEXT_locateCorrectCase(name.data()) != 0)
		throw std::runtime_error("Missing XL named-audio resource in the mission HOG: " + name + "; external sound directories are not implemented");
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

std::vector<AudioSample> convert_bank(const NamedAudioResources &resources, unsigned rate)
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

namespace xl_compat
{
std::shared_ptr<const NamedAudioResources> prepare_named_audio(const char *level, const Inventory &inventory)
{
	auto resources = std::make_shared<NamedAudioResources>();
	const auto add = [&](const Trigger &action, int key) {
		if (action.type != 30) return;
		if (key < 0 || unsigned(key) >= event_limit || action.value / 65536 <= 0)
			throw std::runtime_error("XL named sound requires a positive SND ID and a bounded action owner");
		resources->actions.emplace(key, NamedAudioResources::Action{ action.value / 65536, action.time, 0 });
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
	std::map<std::string, unsigned> by_name;
	std::size_t bytes = 0;
	for (auto &[key, action] : resources->actions) {
		(void) key;
		const auto *row = find_level_text(table, action.id);
		if (!row) throw std::runtime_error("XL sound ID " + std::to_string(action.id) + " is missing from " + name);
		const auto filename = named_sound_filename(row->text);
		const auto previous = by_name.find(filename);
		if (previous != by_name.end()) {
			action.sample = previous->second;
			continue;
		}
		const auto wav = owned_bytes(filename, max_wave_bytes);
		WaveChannels pcm;
		try {
			pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(wav.data()), wav.size());
		} catch (const std::exception &error) {
			throw std::runtime_error("Invalid XL named sound " + filename + ": " + error.what());
		}
		bytes += pcm.samples.size() * sizeof(std::int16_t);
		if (bytes > 128 * 1024 * 1024) throw std::runtime_error("Named-audio bank exceeds 128 MiB of source storage");
		action.sample = unsigned(resources->sources.size());
		by_name.emplace(filename, action.sample);
		resources->sources.push_back(std::move(pcm));
		resources->names.push_back(filename);
		identity += ":" + filename + ":" + picosha2::hash256_hex_string(wav);
	}
	resources->identity = picosha2::hash256_hex_string(identity);
	// Complete output conversion and duration validation before publishing a bank
	resources->prepared_rate = output_rate;
	resources->prepared_samples = std::make_shared<const std::vector<AudioSample>>(convert_bank(*resources, output_rate));
	return resources;
}

void install_named_audio(std::shared_ptr<const NamedAudioResources> resources)
{
	// Preparation and installation run on the game thread without reopening audio
	if (resources && resources->prepared_rate != output_rate) throw std::runtime_error("XL named-audio output changed during level preparation");
	auto samples = resources ? resources->prepared_samples : nullptr;
	std::shared_ptr<const NamedAudioResources> previous;
	{
		AudioLock lock;
		mixer.clear();
		events.fill({});
		previous = std::move(bank);
		bank = std::move(resources);
		converted.swap(samples);
		failure.clear();
	}
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
	float gain = float((std::clamp) (volume, 0, 32768) / 256);
#ifdef __ANDROID__
	gain *= AUDIO_EFFECTS_VOLUME_SCALE * AUDIO_GAMEPLAY_HEADROOM_SCALE;
#endif
	AudioLock lock;
	mixer.volume = int(gain);
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
	if (!resume_pending || game_is_time_paused()) return;
	AudioLock lock;
	mixer.paused = false;
	resume_pending = false;
}
extern "C" void xl_named_audio_stop(void)
{
	AudioLock lock;
	mixer.clear();
}
extern "C" void xl_named_audio_clear(void)
{
	xl_compat::install_named_audio({});
}
extern "C" const char *xl_named_audio_error(void)
{
	return failure.c_str();
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
	const int slot = backend_ready ? mixer.start(authored.sample, authored.repetitions, owner) : -1;
	if (slot < 0) {
		if (!GameArg.SndNoSound) {
			++failed_starts;
			failure = backend_ready ? "XL named-audio voice capacity exhausted" : "XL named-audio output is unavailable";
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
	std::array<AudioVoice, audio_voice_limit> voices;
	std::uint64_t next;
	{
		AudioLock lock;
		voices = mixer.voices;
		next = mixer.next_serial;
	}
	nlohmann::json state = { { "source", bank ? bank->identity : "" }, { "rate", output_rate }, { "next", next }, { "events", nlohmann::json::array() }, { "voices", nlohmann::json::array() } };
	if (bank)
		for (const auto &[key, action] : bank->actions) {
			(void) action;
			const auto &event = events[unsigned(key)];
			const auto elapsed = !event.activated ? -1 : (std::max) (std::int64_t(0), GameTime64 - event.started);
			state["events"].push_back({ key, elapsed, event.cooldown, event.serial, event.rate, event.playing });
		}
	for (unsigned slot = 0; slot < voices.size(); ++slot) {
		const auto &voice = voices[slot];
		if (voice.remaining) state["voices"].push_back({ slot, voice.serial, voice.sample, voice.position, voice.owner, voice.remaining });
	}
	return state;
}

bool restore_named_audio(const nlohmann::json &state, bool apply)
{
	try {
		if (!state.is_object() || state.size() != 5 || !state.at("source").is_string() ||
		    state.at("source").get<std::string>() != (bank ? bank->identity : "") ||
		    !state.at("events").is_array() || state.at("events").size() != (bank ? bank->actions.size() : 0) ||
		    !state.at("voices").is_array() || state.at("voices").size() > audio_voice_limit)
			throw std::runtime_error("Named-audio save has different resources or layout");
		const auto rate = unsigned(integer(state.at("rate"), 8000, 192000));
		const auto next = std::uint64_t(integer(state.at("next"), 1, INT64_MAX));
		std::array<AudioEvent, event_limit> restored_events;
		std::set<unsigned> owners, slots;
		std::set<std::uint64_t> serials, event_serials;
		for (const auto &row : state.at("events")) {
			if (!row.is_array() || row.size() != 6 || !row[5].is_boolean()) throw std::runtime_error("Invalid named-audio event row");
			const auto owner = unsigned(integer(row[0], 0, event_limit - 1));
			if (!bank || !bank->actions.count(int(owner)) || !owners.insert(owner).second) throw std::runtime_error("Unknown or duplicate named-audio action owner");
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
			if (!row.is_array() || row.size() != 6 || !bank) throw std::runtime_error("Invalid named-audio voice row");
			const auto slot = unsigned(integer(row[0], 0, audio_voice_limit - 1));
			if (!slots.insert(slot).second) throw std::runtime_error("Duplicate named-audio voice slot");
			auto &voice = voices[slot];
			voice.serial = std::uint64_t(integer(row[1], 1, std::int64_t(next - 1)));
			voice.sample = unsigned(integer(row[2], 0, std::int64_t(bank->sources.size() - 1)));
			voice.owner = int(integer(row[4], 0, event_limit - 1));
			voice.remaining = int(integer(row[5], -1, INT32_MAX));
			if (!serials.insert(voice.serial).second || !bank->actions.count(voice.owner)) throw std::runtime_error("Invalid named-audio voice identity");
			const auto &action = bank->actions.at(voice.owner);
			const auto &event = restored_events[unsigned(voice.owner)];
			if (action.sample != voice.sample || !event.activated || event.serial < voice.serial ||
			    (action.repetitions > 0 ? (voice.remaining < 1 || voice.remaining > action.repetitions) : voice.remaining != -1))
				throw std::runtime_error("Named-audio voice differs from its authored action");
			const auto frame_count = frames_at_rate(bank->sources.at(voice.sample), rate);
			const auto position = std::uint64_t(integer(row[3], 0, std::int64_t(frame_count - 1)));
			voice.position = unsigned((std::min) (position * output_rate / rate, std::uint64_t(converted->at(voice.sample).frames() - 1)));
		}
		if (apply) {
			AudioLock lock;
			// Do not consume restored samples while the loader is still running
			resume_pending = resume_pending || !mixer.paused;
			mixer.paused = true;
			mixer.voices = voices;
			mixer.next_serial = next;
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
	nlohmann::json result = { { "backend_ready", backend_ready }, { "rate", output_rate }, { "resources", bank ? bank->sources.size() : 0 }, { "actions", bank ? bank->actions.size() : 0 }, { "files", bank ? bank->names : std::vector<std::string>{} }, { "paused", copy.paused }, { "resume_pending", resume_pending }, { "volume", copy.volume }, { "starts", copy.starts }, { "completions", copy.completions }, { "stops", copy.stops }, { "failed_starts", failed_starts }, { "output_callbacks", calls }, { "output_frames", delivered }, { "error", failure }, { "voices", nlohmann::json::array() } };
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
