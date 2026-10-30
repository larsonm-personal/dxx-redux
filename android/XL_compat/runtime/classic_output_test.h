#ifndef DXX_XL_CLASSIC_OUTPUT_TEST_H
#define DXX_XL_CLASSIC_OUTPUT_TEST_H
// Independent labeled fixture PCM for native and Android replacement callbacks
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace xl_classic_output
{
inline void compare(const nlohmann::json &state, int ordinal, const std::int16_t *captured,
                    const std::int16_t *music, std::size_t count, int gain, int replacement_sample = 4)
{
	const int left = ordinal == 1 ? 2500 : ordinal == 2 ? -3000
	                                   : ordinal == -1  ? 11000
	                                                    : 8000;
	const int right = ordinal == 1 ? -7500 : ordinal == 2 ? 9000
	                                     : ordinal == -1  ? -5000
	                                                      : -4000;
	for (std::size_t i = 0; i < count; ++i) {
		int expected = music ? music[i] : 0;
		for (const auto &voice : state.at("voices")) {
			const int sample = voice[2].get<int>();
			if (sample != replacement_sample && sample != 0 && sample != 1) throw std::runtime_error("Classic replacement output contains an unexpected sample");
			const auto frames = state.at("rate").get<unsigned>() / 2;
			const auto phase = voice[3].get<std::size_t>() + i / 2;
			if (voice[5].get<int>() > 0 && phase / frames >= voice[5].get<unsigned>()) continue;
			const int raw = sample != 0 ? (i % 2 ? right : left) : (i % 2 ? -2000 : 1000);
			const int panned = int(float(raw) * (voice[11 + i % 2].get<float>() / 255.0f));
			expected = (std::clamp) (expected + panned * (voice[10].get<int>() * gain / 128) / 128, -32768, 32767);
		}
		if (captured[i] != expected) throw std::runtime_error("Classic replacement actual output differs at sample " + std::to_string(i) +
			                                                  ": " + std::to_string(captured[i]) + " != " + std::to_string(expected) +
			                                                  "; voices=" + state.at("voices").dump());
	}
}
} // namespace xl_classic_output
#endif
