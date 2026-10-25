#ifndef DXX_XL_WAVE_H
#define DXX_XL_WAVE_H
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xl_compat
{
constexpr std::size_t max_wave_bytes = 16 * 1024 * 1024;
struct WavePcm {
	std::uint32_t rate = 0;
	std::uint16_t source_channels = 0, source_bits = 0;
	// Native-endian signed PCM, one sample per source frame, no resampling
	std::vector<std::int16_t> mono;
};
// Bounded PCM RIFF/WAVE only; throws with a reason before publishing samples
WavePcm decode_wave(const std::uint8_t *data, std::size_t size);
// Redux source format; explicit quantization and nearest-frame rate conversion
std::vector<std::uint8_t> wave_unsigned8(const WavePcm &pcm, std::uint32_t rate);
} // namespace xl_compat
#endif
