// Execute the actual XL S16 stock converter; buffer allocation is an adapter
#include "classic_sample.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

namespace classic_sample_reference
{
#define ENTER(a, b)
#define RETVAL(value) return value;
#define RETURN        return;
#define DBG           0
#define MAKE_WAV      0
#define WAVINFO_SIZE  0
constexpr int AUDIO_S16SYS = 0x8010;
struct Buffer {
	std::vector<std::uint16_t> words;
	bool Create(int size)
	{
		if (size < 0 || size > 32 * 1024 * 1024) throw std::runtime_error("Source converter exceeded bounded fixture allocation");
		words.assign((unsigned(size) + 1) / 2, 0x5a5a);
		return true;
	}
	std::uint8_t *BufferPointer()
	{
		return reinterpret_cast<std::uint8_t *>(words.data());
	}
};
struct SampleBuffer : Buffer {
	std::uint8_t *Buffer()
	{
		return BufferPointer();
	}
};
struct CSoundSample {
	int bCustom = 0, nLength[2]{};
	SampleBuffer data[2];
};
struct CAudioChannel {
	struct {
		SampleBuffer sample;
		int bResampled = 0, nLength = 0;
	} m_info;
	int32_t Resample(CSoundSample *, int32_t, int32_t);
};
struct {
	int Format() const
	{
		return AUDIO_S16SYS;
	}
} audio;
struct Options {
	struct {
		int audioSampleRate = 44100, soundSampleRate = 22050;
	} sound;
} options;
auto *gameOpts = &options;
#include "xl_reference_classic_sample.h"
} // namespace classic_sample_reference

void classic_sample_source_controls()
{
	using namespace classic_sample_reference;
	const auto require = [](bool value, const char *message) {if(!value)throw std::runtime_error(message); };
	unsigned controls = 0, values = 0;
	std::mt19937 random(0x584c5358);
	for (unsigned rate : { 11025u, 22050u })
		for (unsigned length : { 1u, 2u, 3u, 127u, 128u, 249u, 250u, 251u, 499u, 500u, 501u, 749u, 750u, 999u, 1000u, 1001u, 4093u })
			for (unsigned pattern = 0; pattern < 5; ++pattern)
				for (int custom : { 0, 1 })
					for (int d1 : { 0, 1 }) {
						std::vector<std::uint8_t> bytes(length);
						for (unsigned i = 0; i < length; ++i) bytes[i] = pattern == 0 ? 0 : pattern == 1 ? 128
							                                                            : pattern == 2   ? 255
							                                                            : pattern == 3   ? std::uint8_t(i)
							                                                                             : std::uint8_t(random());
						CSoundSample sample;
						sample.bCustom = custom;
						sample.nLength[custom] = int(length);
						sample.data[custom].Create(int(length));
						std::memcpy(sample.data[custom].Buffer(), bytes.data(), length);
						options.sound.soundSampleRate = int(rate);
						CAudioChannel channel;
						const auto source_size = channel.Resample(&sample, d1, 0);
						const auto actual = xl_compat::prepare_classic_sample(bytes.data(), bytes.size(), rate, 44100);
						require(source_size == int(actual.stereo.size() * sizeof(std::int16_t)) && channel.m_info.bResampled == 1 && channel.m_info.nLength == source_size, "Classic source conversion length differs");
						require(std::memcmp(actual.stereo.data(), channel.m_info.sample.Buffer(), unsigned(source_size)) == 0, "Classic source conversion bytes or endpoint fade differ");
						++controls;
						values += unsigned(actual.stereo.size());
					}
	for (unsigned rate : { 11025u, 22050u }) {
		std::vector<std::uint8_t> bytes(1001, 255);
		const auto base = xl_compat::prepare_classic_sample(bytes.data(), bytes.size(), rate, 44100);
		for (unsigned output : { 8000u, 22050u, 48000u, 96000u, 192000u }) {
			const auto converted = xl_compat::prepare_classic_sample(bytes.data(), bytes.size(), rate, output);
			require(converted.rate == output && converted.frames() == (base.frames() * output + 22050) / 44100, "Device conversion changed canonical duration");
			for (std::size_t i = 0; i < converted.frames(); ++i) require(converted.stereo[i * 2] == converted.stereo[i * 2 + 1], "Device conversion changed classic stereo duplication");
			++controls;
		}
	}
	unsigned rejected = 0;
	const std::uint8_t byte = 128;
	const auto reject = [&](const std::uint8_t *data, std::size_t count, unsigned input, unsigned output) {
		try {
			xl_compat::prepare_classic_sample(data, count, input, output);
		} catch (const std::exception &) {
			++rejected;
			return;
		}
		throw std::runtime_error("Undefined or oversized classic sample was accepted");
	};
	reject(nullptr, 1, 22050, 44100);
	reject(&byte, 0, 22050, 44100);
	reject(&byte, 1, 0, 44100);
	reject(&byte, 1, 44100, 44100);
	reject(&byte, 1, 22050, 7999);
	reject(&byte, 1, 22050, 192001);
	reject(&byte, 8u * 1024u * 1024u, 11025, 44100);
	std::cout << "PASS: " << controls << " classic conversion/device controls, " << values << " exact source PCM values and " << rejected << " rejected inputs; U8/OpenAL profiles remain separate\n";
}
