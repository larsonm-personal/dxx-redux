#include "wave.h"
#include <cstring>
#include <stdexcept>

namespace xl_compat
{
namespace
{
void require(bool condition, const char *reason)
{
	if (!condition) throw std::runtime_error(reason);
}
std::uint16_t u16(const std::uint8_t *p)
{
	return std::uint16_t(p[0] | (unsigned(p[1]) << 8));
}
std::uint32_t u32(const std::uint8_t *p)
{
	return std::uint32_t(u16(p)) | (std::uint32_t(u16(p + 2)) << 16);
}
int sample(const std::uint8_t *p, unsigned bits)
{
	if (bits == 8) return (int(*p) - 128) * 256;
	const int value = u16(p);
	return value >= 32768 ? value - 65536 : value;
}
} // namespace

WavePcm decode_wave(const std::uint8_t *data, std::size_t size)
{
	require(data && size >= 12 && size <= max_wave_bytes, "WAV size is outside the supported bound");
	require(!std::memcmp(data, "RIFF", 4) && !std::memcmp(data + 8, "WAVE", 4), "Expected little-endian RIFF/WAVE");
	require(std::uint64_t(u32(data + 4)) + 8 == size, "WAV RIFF length does not match the file");
	const std::uint8_t *format = nullptr, *samples = nullptr;
	std::size_t sample_bytes = 0;
	unsigned chunks = 0;
	for (std::size_t offset = 12; offset < size;) {
		require(++chunks <= 1024, "WAV has too many chunks");
		require(size - offset >= 8, "WAV chunk header is truncated");
		const auto *header = data + offset;
		const auto length = std::size_t(u32(header + 4));
		offset += 8;
		require(length <= size - offset, "WAV chunk payload is truncated");
		if (!std::memcmp(header, "fmt ", 4)) {
			require(!format, "WAV has duplicate format chunks");
			require(length == 16 || (length == 18 && u16(data + offset + 16) == 0), "WAV format extensions are unsupported");
			format = data + offset;
		} else if (!std::memcmp(header, "data", 4)) {
			require(!samples, "WAV has duplicate data chunks");
			samples = data + offset;
			sample_bytes = length;
		}
		offset += length;
		if (length & 1) {
			require(offset < size, "WAV odd chunk is missing its padding byte");
			++offset;
		}
	}
	require(format && samples && sample_bytes, "WAV requires format and nonempty data chunks");
	require(u16(format) == 1, "Only PCM WAV effects are supported");
	WavePcm result;
	result.source_channels = u16(format + 2);
	result.rate = u32(format + 4);
	result.source_bits = u16(format + 14);
	require(result.source_channels == 1 || result.source_channels == 2, "WAV must be mono or stereo");
	require(result.source_bits == 8 || result.source_bits == 16, "WAV PCM must be 8 or 16 bit");
	require(result.rate >= 8000 && result.rate <= 192000, "WAV sample rate is outside the supported range");
	const unsigned stride = result.source_channels * result.source_bits / 8;
	require(u16(format + 12) == stride && u32(format + 8) == result.rate * stride, "WAV PCM alignment or byte rate is inconsistent");
	require(sample_bytes % stride == 0, "WAV data ends inside a sample frame");
	const auto frames = sample_bytes / stride;
	require(frames <= max_wave_bytes / sizeof(std::int16_t), "Decoded WAV exceeds the sample budget");
	result.mono.resize(frames);
	for (std::size_t i = 0; i < frames; ++i) {
		const auto *p = samples + i * stride;
		int value = sample(p, result.source_bits);
		if (result.source_channels == 2) value = (value + sample(p + result.source_bits / 8, result.source_bits)) / 2;
		result.mono[i] = static_cast<std::int16_t>(value);
	}
	return result;
}
std::vector<std::uint8_t> wave_unsigned8(const WavePcm &pcm, std::uint32_t rate)
{
	require(rate >= 8000 && rate <= 192000 && pcm.rate >= 8000 && pcm.rate <= 192000,
	        "Invalid WAV playback rate");
	require(!pcm.mono.empty() && pcm.mono.size() <= max_wave_bytes / sizeof(std::int16_t), "Invalid WAV playback frame count");
	const auto count = static_cast<std::uint64_t>(pcm.mono.size()) * rate / pcm.rate;
	require(count && count <= max_wave_bytes, "Converted WAV exceeds playback limits");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count));
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		const auto index = static_cast<std::size_t>(static_cast<std::uint64_t>(i) * pcm.rate / rate);
		bytes[i] = static_cast<std::uint8_t>((static_cast<int>(pcm.mono[index]) + 32768) / 256);
	}
	return bytes;
}
} // namespace xl_compat
