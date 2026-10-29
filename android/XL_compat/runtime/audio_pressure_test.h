#ifndef DXX_XL_AUDIO_PRESSURE_TEST_H
#define DXX_XL_AUDIO_PRESSURE_TEST_H
// Native and Android diagnostics for the labeled synthetic named-audio fixture
// Include after the engine's game and trigger declarations
#include "named_audio.h"
#include "named_audio_state.h"
#include "../audio_voice.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace xl_audio_pressure
{
inline void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}

inline void replace(int owner, unsigned slot)
{
	const auto before = xl_compat::named_audio_snapshot();
	require(before.at("voices").size() == xl_compat::audio_voice_limit, "Pressure replacement requires a full voice pool");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[owner], owner) == 1, "Full-pool named request failed");
	const auto after = xl_compat::named_audio_snapshot();
	require(after.at("voices").size() == xl_compat::audio_voice_limit &&
	            after.at("cursor") == (slot + 1) % xl_compat::audio_voice_limit &&
	            after.at("next") == before.at("next").get<std::uint64_t>() + 1,
	        "Pressure replacement changed occupancy, serial or allocation position");
	for (unsigned i = 0; i < xl_compat::audio_voice_limit; ++i) {
		const auto &voice = after.at("voices")[i];
		require(voice[0] == i, "Full-pool voice slots are incomplete");
		if (i == slot)
			require(voice[1] == before.at("next") && voice[2] == owner && voice[3] == 0 && voice[4] == owner && voice[5] == -1 && voice[6] == 32768,
			        "Pressure replacement did not retire the expected owner and playhead");
		else require(voice == before.at("voices")[i], "Pressure replacement changed another live voice");
	}
}

inline unsigned prepare()
{
	const auto before = xl_compat::named_audio_snapshot();
	require(before.at("voices").empty() && Triggers[1].type == 30 && Triggers[1].time == 0 && Triggers[2].type == 30 && Triggers[2].time == -1,
	        "Channel pressure requires fresh zero/negative actions from the derived fixture");
	const unsigned origin = before.at("cursor").get<unsigned>();
	const auto serial = before.at("next").get<std::uint64_t>();
	for (unsigned i = 0; i < xl_compat::audio_voice_limit; ++i) {
		const int owner = i < 63 ? 2 : 1;
		++GameTime64;
		require(xl_named_audio_action(&Triggers[owner], owner) == 1, "Cannot fill the source-sized named voice pool");
	}
	const auto full = xl_compat::named_audio_snapshot();
	require(full.at("voices").size() == xl_compat::audio_voice_limit && full.at("cursor") == origin && full.at("next") == serial + 128,
	        "Filling the named pool lost a voice or circular allocation position");
	for (unsigned i = 0; i < xl_compat::audio_voice_limit; ++i) {
		const auto &voice = full.at("voices")[(origin + i) % xl_compat::audio_voice_limit];
		require(voice[1] == serial + i && voice[4] == (i < 63 ? 2 : 1), "Named full-pool birth order differs");
	}
	// Below half the pool, a new persistent voice replaces a nonpersistent one
	replace(2, (origin + 63) % xl_compat::audio_voice_limit);
	// At half, the class is isolated even though the other class has unprotected voices
	replace(2, origin);
	replace(1, (origin + 64) % xl_compat::audio_voice_limit);
	return origin;
}

inline void continuation(unsigned origin)
{
	// Choosing a zero-repeat voice first makes a reset cursor pick another slot
	replace(1, (origin + 65) % xl_compat::audio_voice_limit);
	replace(2, origin);
}

inline void output(const nlohmann::json &state, const std::int16_t *samples, const std::int16_t *music,
                   std::size_t count, unsigned rate, int gain)
{
	struct Voice {
		unsigned position, source_rate, length;
		bool mono;
	};
	std::array<Voice, xl_compat::audio_voice_limit> voices;
	require(state.at("voices").size() == voices.size(), "Pressure output requires all authored voices");
	for (unsigned i = 0; i < voices.size(); ++i) {
		const auto &row = state.at("voices")[i];
		require(row[0] == i && (row[4] == 1 || row[4] == 2) && row[5] == -1, "Pressure output has an unexpected owner or repetition");
		const bool mono = row[4] == 2;
		const unsigned source_rate = mono ? 22050 : 44100;
		voices[i] = { row[3].get<unsigned>(), source_rate, unsigned((std::uint64_t(source_rate / 2) * rate + source_rate / 2) / source_rate), mono };
	}
	const auto raw = [](std::uint64_t frame, unsigned channel) {
		return int((frame * (channel ? 53 : 37) + (channel ? 3 : 0)) % 511) * 100 - 25500;
	};
	// Evaluate the independent fixture waveform at output timestamps, in slot order
	for (std::size_t i = 0; i < count; ++i) {
		int expected = music ? music[i] : 0;
		for (const auto &voice : voices) {
			const auto phase = std::uint64_t((voice.position + i / 2) % voice.length) * voice.source_rate;
			const auto first = (std::min) (phase / rate, std::uint64_t(voice.source_rate / 2 - 1));
			const auto second = (std::min) (first + 1, std::uint64_t(voice.source_rate / 2 - 1));
			const auto fraction = std::int64_t(phase % rate);
			const unsigned channel = voice.mono ? 0 : unsigned(i % 2);
			const auto value = (std::int64_t(raw(first, channel)) * (rate - fraction) + std::int64_t(raw(second, channel)) * fraction) / rate;
			const int panned = int(float(value) * (127.0f / 255.0f));
			expected = (std::clamp) (expected + panned * gain / 128, -32768, 32767);
		}
		if (samples[i] != expected)
			throw std::runtime_error("Pressure PCM differs at sample " + std::to_string(i) + ": " + std::to_string(samples[i]) + " != " + std::to_string(expected));
	}
}
} // namespace xl_audio_pressure
#endif
