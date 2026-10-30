#include "classic_sample.h"
#include <stdexcept>

namespace xl_compat
{
AudioSample prepare_classic_sample(const std::uint8_t *bytes, std::size_t length, unsigned source_rate, unsigned output_rate)
{
	constexpr unsigned canonical_rate = 44100;
	if (!bytes || !length || (source_rate != 11025 && source_rate != 22050) || output_rate < 8000 || output_rate > 192000)
		throw std::runtime_error("Invalid XL classic sample format");
	const unsigned ratio = canonical_rate / source_rate;
	if (length > (8u * 1024u * 1024u) / ratio)
		throw std::runtime_error("XL classic sample exceeds the frame bound");
	std::vector<std::uint16_t> mono(length * ratio);
	// ResampleFormat deliberately scales unsigned bytes without a signed offset
	for (std::size_t i = 0; i < length; ++i) mono[i] = std::uint16_t(unsigned(bytes[i]) * 128u);
	std::size_t count = length;
	for (unsigned remaining = ratio; remaining > 1; remaining >>= 1) {
		std::uint16_t next = mono[count - 1];
		for (std::size_t i = count; i; --i) {
			const auto current = mono[i - 1];
			mono[2 * i - 1] = std::uint16_t((unsigned(current) + next) / 2);
			mono[2 * i - 2] = current;
			next = current;
		}
		count *= 2;
	}
	WaveChannels canonical{ canonical_rate, 2, 16, {} };
	canonical.samples.resize(count * 2);
	for (std::size_t i = 1; i <= count; ++i) {
		auto value = mono[i - 1];
		float fade = float(i) / 500.0f;
		if (fade > 1.0f) fade = float(count - i) / 500.0f;
		if (fade < 1.0f) value = std::uint16_t(float(value) * fade);
		canonical.samples[2 * i - 2] = canonical.samples[2 * i - 1] = std::int16_t(value);
	}
	if (output_rate == canonical_rate) return { canonical_rate, std::move(canonical.samples) };
	return prepare_audio_sample(canonical, output_rate);
}
} // namespace xl_compat
