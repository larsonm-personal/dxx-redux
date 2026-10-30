#ifndef DXX_XL_SHARED_AUDIO_PCM_TEST_H
#define DXX_XL_SHARED_AUDIO_PCM_TEST_H
// Diagnostic oracle for the labeled fixture; stock conversion is source-qualified separately
#include "classic_audio.h"
#include "../audio_voice.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
namespace xl_shared_audio_pcm
{
inline std::vector<xl_compat::AudioSample> reference_bank(unsigned rate)
{
	std::vector<xl_compat::AudioSample> result;
	for (unsigned index = 0; index < 4; ++index) {
		const unsigned channels = index == 2 ? 1u : 2u, source_rate = index == 2 ? 22050u : 44100u;
		const unsigned frames = index == 1 ? 44100u : index == 2 ? 11025u
		                                          : index == 3   ? 1u
		                                                         : 22050u;
		xl_compat::WaveChannels pcm{ source_rate, std::uint16_t(channels), 16, {} };
		for (unsigned frame = 0; frame < frames; ++frame)
			for (unsigned channel = 0; channel < channels; ++channel)
				pcm.samples.push_back(std::int16_t(int((frame * (channel ? 53 : 37) + (channel ? 3 : 0)) % 511) * 100 - 25500));
		result.push_back(xl_compat::prepare_audio_sample(pcm, rate));
	}
	// Stock conversion has an independent pinned-source PCM comparison
	for (const auto &sample : xl_compat::prepare_classic_playback({}).samples)
		result.push_back(xl_compat::prepare_audio_sample(sample.pcm, rate));
	return result;
}
inline void output(const nlohmann::json &state, const std::int16_t *captured, const std::int16_t *music, std::size_t captured_samples, int gain)
{
	const auto samples = reference_bank(state.at("rate").get<unsigned>());
	for (std::size_t i = 0; i < captured_samples; ++i) {
		int expected = music ? music[i] : 0;
		for (const auto &row : state.at("voices")) {
			const auto &sample = samples.at(row[2].get<unsigned>());
			const auto phase = row[3].get<std::size_t>() + i / 2;
			if (row[5].get<int>() > 0 && phase / sample.frames() >= row[5].get<unsigned>()) continue;
			const auto raw = sample.stereo[(phase % sample.frames()) * 2 + i % 2];
			const int panned = int(float(raw) * (row[11 + i % 2].get<float>() / 255.0f));
			expected = (std::clamp) (expected + panned * (row[10].get<int>() * gain / 128) / 128, -32768, 32767);
		}
		if (captured[i] != expected) throw std::runtime_error("Shared classic/named output differs at sample " + std::to_string(i));
	}
}
} // namespace xl_shared_audio_pcm
#endif
