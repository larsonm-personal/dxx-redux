#include "audio_voice.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace xl_compat
{
AudioSample prepare_audio_sample(const WaveChannels &source, unsigned rate)
{
	if (!source.rate || (source.source_channels != 1 && source.source_channels != 2) ||
	    source.samples.empty() || source.samples.size() % source.source_channels || rate < 8000 || rate > 192000)
		throw std::runtime_error("Invalid named-audio sample format");
	const auto source_frames = source.samples.size() / source.source_channels;
	if (source_frames > 8 * 1024 * 1024) throw std::runtime_error("Named-audio input exceeds the frame bound");
	const auto frames = (std::max) (std::uint64_t(1), (std::uint64_t(source_frames) * rate + source.rate / 2) / source.rate);
	if (frames > 8 * 1024 * 1024) throw std::runtime_error("Named-audio conversion exceeds the frame bound");
	AudioSample result;
	result.rate = rate;
	result.stereo.resize(std::size_t(frames) * 2);
	for (std::uint64_t frame = 0; frame < frames; ++frame) {
		const auto phase = frame * source.rate;
		const auto first = (std::min) (phase / rate, std::uint64_t(source_frames - 1));
		const auto second = (std::min) (first + 1, std::uint64_t(source_frames - 1));
		const auto fraction = std::int64_t(phase % rate);
		for (unsigned channel = 0; channel < 2; ++channel) {
			const unsigned source_channel = source.source_channels == 1 ? 0 : channel;
			const std::int64_t a = source.samples[first * source.source_channels + source_channel];
			const std::int64_t b = source.samples[second * source.source_channels + source_channel];
			result.stereo[std::size_t(frame) * 2 + channel] = std::int16_t((a * (rate - fraction) + b * fraction) / rate);
		}
	}
	return result;
}

int AudioVoices::start(unsigned sample, int repetitions, int owner)
{
	if (owner < 0 || !next_serial || next_serial >= std::uint64_t(INT64_MAX)) return -1;
	for (unsigned slot = 0; slot < voices.size(); ++slot) {
		if (voices[slot].remaining) continue;
		voices[slot] = { next_serial++, sample, 0, owner, repetitions <= 0 ? -1 : repetitions };
		++starts;
		return int(slot);
	}
	return -1;
}

void AudioVoices::stop(int slot)
{
	if (slot < 0 || unsigned(slot) >= voices.size() || !voices[slot].remaining) return;
	voices[slot].remaining = 0;
	++stops;
}

void AudioVoices::clear()
{
	for (unsigned slot = 0; slot < voices.size(); ++slot) stop(int(slot));
	voices.fill({});
}

void AudioVoices::mix(unsigned char *stream, std::size_t frames, const std::vector<AudioSample> &samples)
{
	if (!stream || paused) return;
	const int gain = (std::clamp) (volume, 0, 128);
	for (auto &voice : voices) {
		if (!voice.remaining) continue;
		if (voice.sample >= samples.size() || samples[voice.sample].stereo.empty() ||
		    voice.position >= samples[voice.sample].frames()) {
			voice.remaining = 0;
			++stops;
			continue;
		}
		const auto &sample = samples[voice.sample];
		for (std::size_t frame = 0; frame < frames && voice.remaining; ++frame) {
			for (unsigned channel = 0; channel < 2; ++channel) {
				std::int16_t previous;
				auto *destination = stream + (frame * 2 + channel) * sizeof(previous);
				std::memcpy(&previous, destination, sizeof(previous));
				// Source Mix_VolPan rounds the centered pan to 127 on both sides
				const int panned = int(float(sample.stereo[std::size_t(voice.position) * 2 + channel]) * (127.0f / 255.0f));
				const auto mixed = std::int16_t((std::clamp) (int(previous) + panned * gain / 128, -32768, 32767));
				std::memcpy(destination, &mixed, sizeof(mixed));
			}
			if (++voice.position == sample.frames()) {
				voice.position = 0;
				if (voice.remaining > 0 && !--voice.remaining) ++completions;
			}
		}
	}
}
} // namespace xl_compat
