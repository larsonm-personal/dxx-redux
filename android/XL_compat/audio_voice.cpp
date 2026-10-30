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

AudioPriority audio_priority(int sound_class, int requested_volume, int effective_volume, int sound_object, bool looping)
{
	const bool persistent = sound_object >= 0 || looping || requested_volume > 65536;
	return { !sound_class && persistent ? -1 : sound_class, effective_volume, persistent };
}

AudioOutput audio_start_output(int effective_volume, int pan, AudioOutput previous)
{
	// The source doubles this signed value before conversion to float
	if (effective_volume < 0 || effective_volume > INT32_MAX / 2) throw std::runtime_error("XL audio volume exceeds the defined source range");
	previous.volume = (std::min) (128, int(float(2 * effective_volume) / 65536.0f * 128.0f + 0.5f));
	if (pan >= 0) {
		const int left = int(float(pan) / 65536.0f * 254.0f + 0.5f);
		previous.pan = { std::uint8_t(left), std::uint8_t(254 - left) };
	}
	return previous;
}

int AudioVoices::start(unsigned sample, int repetitions, int owner)
{
	// Named voices have equal requested volume; zero repeats loop without priority
	return start(sample, repetitions, owner, audio_priority(0, 65536, 32768, -1, repetitions < 0));
}

int AudioVoices::start(unsigned sample, int repetitions, int owner, AudioPriority priority, AudioOutput output)
{
	if (owner < 0 || !next_serial || next_serial >= std::uint64_t(INT64_MAX) || cursor >= voices.size() || priority.volume < 0 || output.volume < 0 || output.volume > 128) return -1;
	const auto class_count = std::count_if(voices.begin(), voices.end(), [&](const auto &voice) {
		return voice.remaining && voice.priority.sound_class == priority.sound_class;
	});
	const bool use_class = class_count >= int(voices.size() / 2);
	int selected = -1, minimum[2]{ -1, -1 };
	for (unsigned step = 0; step < voices.size(); ++step) {
		const unsigned slot = (cursor + step) % unsigned(voices.size());
		const auto &voice = voices[slot];
		if (!voice.remaining) {
			selected = int(slot);
			break;
		}
		if (use_class && voice.priority.sound_class != priority.sound_class) continue;
		auto &candidate = minimum[voice.priority.persistent ? 1 : 0];
		if (candidate < 0 || voices[unsigned(candidate)].priority.volume > voice.priority.volume) candidate = int(slot);
	}
	if (selected < 0) selected = minimum[0] >= 0 ? minimum[0] : minimum[1];
	if (selected < 0) return -1;
	stop(selected);
	voices[unsigned(selected)] = { next_serial++, sample, 0, owner, repetitions <= 0 ? -1 : repetitions, priority, output };
	cursor = (unsigned(selected) + 1) % unsigned(voices.size());
	++starts;
	return selected;
}

bool AudioVoices::set_volume(int slot, int requested_volume, int effects_volume)
{
	if (slot < 0 || unsigned(slot) >= voices.size() || !voices[unsigned(slot)].remaining || requested_volume < 0 || effects_volume < 0 || effects_volume > 65536) return false;
	const auto effective = std::int64_t(requested_volume) * effects_volume / 65536;
	if (effective > INT32_MAX / 2) return false;
	auto &voice = voices[unsigned(slot)];
	if (voice.priority.volume != effective) {
		voice.priority.volume = int(effective);
		voice.output = audio_start_output(int(effective), -1, voice.output);
	}
	return true;
}

bool AudioVoices::set_pan(int slot, int pan)
{
	if (slot < 0 || unsigned(slot) >= voices.size() || !voices[unsigned(slot)].remaining) return false;
	const int left = pan / (32767 / 127);
	voices[unsigned(slot)].output.pan = { std::uint8_t(left), std::uint8_t(254 - left) };
	return true;
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
		const int channel_gain = (std::clamp) (voice.output.volume, 0, 128) * gain / 128;
		for (std::size_t frame = 0; frame < frames && voice.remaining; ++frame) {
			for (unsigned channel = 0; channel < 2; ++channel) {
				std::int16_t previous;
				auto *destination = stream + (frame * 2 + channel) * sizeof(previous);
				std::memcpy(&previous, destination, sizeof(previous));
				const int panned = int(float(sample.stereo[std::size_t(voice.position) * 2 + channel]) * (float(voice.output.pan[channel]) / 255.0f));
				const auto mixed = std::int16_t((std::clamp) (int(previous) + panned * channel_gain / 128, -32768, 32767));
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
