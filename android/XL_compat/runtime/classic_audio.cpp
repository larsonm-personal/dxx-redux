#include "classic_audio.h"
#include "audio_resource.h"
#include "../classic_sample.h"
#include <map>
#include "picosha2.h"
#include <stdexcept>
#ifdef USE_SDLMIXER
#include <SDL_mixer.h>
#endif
extern "C" {
#include "inferno.h"
#include "gr.h"
#include "piggy.h"
#include "args.h"
}

namespace xl_compat
{
int classic_audio_output_rate()
{
#ifdef USE_SDLMIXER
	int rate, channels;
	Uint16 format;
	if (!GameArg.SndDisableSdlMixer && Mix_QuerySpec(&rate, &format, &channels)) return rate;
#endif
	return GameArg.SndDigiSampleRate > 0 ? GameArg.SndDigiSampleRate : 22050;
}

ClassicAudioResources prepare_classic_audio(const char *level, int ordinal, int bank_rate)
{
	ClassicAudioResources result;
	auto scope = prepare_classic_audio_scope(level, ordinal, bank_rate);
	if (!scope) return result;
	std::string identity;
	std::size_t storage = 0;
	for (int sample = 0; const char *name = piggy_sound_name(sample); ++sample) {
		if (!*name) throw std::runtime_error("XL sound replacement requires a named native sample registry");
		const auto resource = load_classic_audio_resource(name, *scope);
		if (!resource) continue;
		WaveChannels pcm;
		try {
			pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource->bytes.data()), resource->bytes.size());
		} catch (const std::exception &error) {
			throw std::runtime_error("Invalid XL classic sound " + resource->path + ": " + error.what());
		}
		storage += pcm.samples.size() * sizeof(std::int16_t);
		if (storage > 128 * 1024 * 1024) throw std::runtime_error("XL classic replacement bank exceeds 128 MiB of source storage");
		const auto hash = picosha2::hash256_hex_string(resource->bytes);
		identity += ":" + std::to_string(sample) + ":" + name + ":" + resource->path + ":" + hash;
		result.samples.push_back({ sample, name, resource->path, hash, std::move(pcm) });
	}
	if (!result.samples.empty()) result.identity = picosha2::hash256_hex_string(identity);
	return result;
}
ClassicAudioResources prepare_classic_playback(const ClassicAudioResources &replacements)
{
	std::map<int, const ClassicAudioSample *> selected;
	for (const auto &sample : replacements.samples) {
		const char *name = piggy_sound_name(sample.sample);
		if (!name || sample.name != name || !selected.emplace(sample.sample, &sample).second)
			throw std::runtime_error("XL classic replacement does not identify a unique native sample");
	}
	ClassicAudioResources result;
	std::string identity = "xl-classic-s16-44100-v1";
	std::size_t storage = 0;
	for (int index = 0; const char *name = piggy_sound_name(index); ++index) {
		ClassicAudioSample sample;
		const auto replacement = selected.find(index);
		if (replacement != selected.end()) sample = *replacement->second;
		else {
			const auto &native = GameSounds[index];
			if (native.bits != 8 || !native.data || native.data == reinterpret_cast<const unsigned char *>(-1) || native.length <= 0)
				throw std::runtime_error("XL classic playback requires loaded 8-bit native sample " + std::to_string(index) + " (" + name + ")");
			AudioSample prepared;
			try {
				prepared = prepare_classic_sample(native.data, std::size_t(native.length), unsigned(native.freq), 44100);
			} catch (const std::exception &error) {
				throw std::runtime_error("XL classic sample " + std::to_string(index) + " (" + name + "): " + error.what());
			}
			const std::string bytes(reinterpret_cast<const char *>(native.data), std::size_t(native.length));
			sample = { index, name, std::string("native:") + name, picosha2::hash256_hex_string(bytes), { 44100, 2, 16, std::move(prepared.stereo) } };
			// Include the original bank rate: equal bytes can represent different durations
			identity += ":" + std::to_string(native.freq);
		}
		storage += sample.pcm.samples.size() * sizeof(std::int16_t);
		if (storage > 128 * 1024 * 1024)
			throw std::runtime_error("XL complete classic bank exceeds 128 MiB of source storage");
		identity += ":" + std::to_string(index) + ":" + sample.name + ":" + sample.path + ":" + sample.source_hash;
		result.samples.push_back(std::move(sample));
	}
	if (result.samples.empty()) throw std::runtime_error("XL classic playback requires a populated native sample registry");
	result.identity = picosha2::hash256_hex_string(identity);
	return result;
}
} // namespace xl_compat
