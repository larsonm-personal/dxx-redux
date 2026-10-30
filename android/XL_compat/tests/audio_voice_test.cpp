#include "audio_voice.h"
#include "named_sound.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

using namespace xl_compat;
static void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
template <class Operation>
static void rejects(Operation operation)
{
	bool failed = false;
	try {
		operation();
	} catch (const std::exception &) {
		failed = true;
	}
	require(failed, "Malformed input was accepted");
}

int main()
{
	try {
		WaveChannels source{ 44100, 2, 16, { 255, -255, 510, -510, 765, -765, 1020, -1020 } };
		std::vector<AudioSample> bank{ prepare_audio_sample(source, 44100) };
		require(bank[0].stereo == source.samples, "Equal-rate conversion changed stereo samples");
		const auto doubled = prepare_audio_sample(source, 88200);
		const std::vector<std::int16_t> expected{ 255, -255, 382, -382, 510, -510, 637, -637, 765, -765, 892, -892, 1020, -1020, 1020, -1020 };
		require(doubled.stereo == expected, "Signed linear conversion or final-frame hold differs");
		const auto mono = prepare_audio_sample(WaveChannels{ 8000, 1, 8, { -32768, 0, 32512 } }, 8000);
		require(mono.stereo == std::vector<std::int16_t>{ -32768, -32768, 0, 0, 32512, 32512 }, "Mono was not duplicated into authored stereo order");
		rejects([&] { prepare_audio_sample(source, 0); });
		rejects([&] { prepare_audio_sample(source, 384000); });
		rejects([&] { prepare_audio_sample(WaveChannels{}, 44100); });
		rejects([&] { prepare_audio_sample(WaveChannels{ 44100, 2, 16, { 1 } }, 44100); });

		AudioVoices voices;
		const int slot = voices.start(0, 2, 3);
		require(slot == 0, "First voice slot was not available");
		std::array<std::int16_t, 24> output{};
		voices.mix(reinterpret_cast<unsigned char *>(output.data()), 3, bank);
		require(voices.voices[0].position == 3 && voices.voices[0].remaining == 2, "First partial callback lost its playhead");
		const auto held = voices.voices;
		voices.paused = true;
		const auto before_pause = output;
		voices.mix(reinterpret_cast<unsigned char *>(output.data()), 4, bank);
		require(output == before_pause && voices.voices[0].position == held[0].position, "Paused voice emitted or advanced");
		AudioVoices paused_bank = voices;
		paused_bank.clear();
		require(paused_bank.paused && !paused_bank.voices[0].remaining && paused_bank.cursor == voices.cursor, "Retiring a sound bank changed pause or allocation position");
		voices.paused = false;
		voices.mix(reinterpret_cast<unsigned char *>(output.data() + 6), 9, bank);
		const std::array<std::int16_t, 24> repeated{ 127, -127, 254, -254, 381, -381, 508, -508, 127, -127, 254, -254, 381, -381, 508, -508, 0, 0, 0, 0, 0, 0, 0, 0 };
		require(output == repeated && voices.completions == 1 && !voices.voices[0].remaining, "Finite repeats or completion boundary differ");
		for (const int repetitions : { 0, -1, -100 }) {
			voices.clear();
			const int loop_slot = voices.start(0, repetitions, 3);
			output.fill(0);
			voices.mix(reinterpret_cast<unsigned char *>(output.data()), 12, bank);
			require(loop_slot >= 0 && voices.voices[unsigned(loop_slot)].remaining == -1 && voices.voices[unsigned(loop_slot)].position == 0, "Zero/negative repeats did not loop indefinitely");
			require(output[16] == 127 && output[23] == -508, "Infinite playback has a gap at a loop boundary");
		}
		voices.clear();
		voices.start(0, 1, 2);
		voices.start(0, 1, 7);
		output.fill(0);
		voices.mix(reinterpret_cast<unsigned char *>(output.data()), 4, bank);
		require(output[0] == 254 && output[7] == -1016, "Independent overlapping voices were not mixed");
		voices.clear();
		const unsigned origin = voices.cursor;
		for (unsigned i = 0; i < audio_voice_limit; ++i) require(voices.start(0, -1, int(i)) == int((origin + i) % audio_voice_limit), "Voice allocation lost its circular order");
		const auto serial = voices.voices[origin].serial;
		require(voices.start(0, 1, 200) == int(origin) && voices.voices[origin].serial != serial && voices.voices[origin].owner == 200,
		        "Full-pool replacement retained a retired owner or changed its tie order");
		output.fill(32000);
		voices.mix(reinterpret_cast<unsigned char *>(output.data()), 4, bank);
		require(output[0] == 32767 && output[7] == -32768, "Output clipping wrapped instead of saturating");
		voices.clear();
		voices.volume = 0;
		const int muted_slot = voices.start(0, 1, 3);
		output.fill(0);
		voices.mix(reinterpret_cast<unsigned char *>(output.data()), 4, bank);
		require(muted_slot >= 0 && std::all_of(output.begin(), output.end(), [](auto value) { return value == 0; }) && !voices.voices[unsigned(muted_slot)].remaining, "Muted playback must advance without output");

		require(named_sound_filename("08:anything") == "lightning.wav", "Numeric addon aliases differ");
		require(named_sound_filename("dripping-water-2") == "dripping-water-2.wav", "Addon prefix lookup differs");
		require(named_sound_filename("Narration.wav") == "Narration.wav", "An ordinary filename was remapped");
		rejects([] { named_sound_filename("43:invalid.wav"); });
		rejects([] { named_sound_filename("../foreign.wav"); });
		rejects([] { named_sound_filename(""); });
		std::cout << "PASS: named stereo voices, conversion, repeat boundaries, pause, overlap, capacity, volume and aliases\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
