#ifndef DXX_XL_BANK_OUTPUT_TEST_H
#define DXX_XL_BANK_OUTPUT_TEST_H
// Independent constant-byte DSND fixture oracle, including endpoint fades
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace xl_bank_output
{
inline void compare(const nlohmann::json &state, const std::int16_t *captured, const std::int16_t *music,
                    std::size_t count, int gain, int sample, unsigned byte, unsigned source_rate, unsigned source_frames)
{
	const auto rate = state.at("rate").get<unsigned>();
	const std::uint64_t canonical_frames = std::uint64_t(source_frames) * (44100 / source_rate);
	const auto frames = (canonical_frames * rate + 22050) / 44100;
	const auto canonical = [&](std::uint64_t frame) {
		frame = (std::min) (frame, canonical_frames - 1);
		const auto edge = frame < 500 ? frame + 1 : canonical_frames - frame - 1;
		return edge < 500 ? unsigned(float(byte * 128) * (float(edge) / 500.0f)) : byte * 128;
	};
	for (std::size_t i = 0; i < count; ++i) {
		int expected = music ? music[i] : 0;
		for (const auto &voice : state.at("voices")) {
			if (voice[2] != sample) throw std::runtime_error("Bank output contains an unexpected sample");
			auto phase = voice[3].get<std::uint64_t>() + i / 2;
			if (voice[5].get<int>() > 0 && phase / frames >= voice[5].get<unsigned>()) continue;
			phase = phase % frames * 44100;
			const auto raw = (std::uint64_t(canonical(phase / rate)) * (rate - phase % rate) +
			                  std::uint64_t(canonical(phase / rate + 1)) * (phase % rate)) /
			                 rate;
			const int panned = int(float(raw) * (voice[11 + i % 2].get<float>() / 255.0f));
			expected = (std::clamp) (expected + panned * (voice[10].get<int>() * gain / 128) / 128, -32768, 32767);
		}
		if (captured[i] != expected) throw std::runtime_error("Bank actual output differs at sample " + std::to_string(i) + ": " + std::to_string(captured[i]) + " != " + std::to_string(expected));
	}
}
} // namespace xl_bank_output
#endif
