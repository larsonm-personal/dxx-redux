// Compare the XL voice output with SDL_mixer's actual centered channel path
#include "audio_voice.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
std::array<std::int16_t, 32768> captured{};
std::size_t captured_samples = 0;
bool overflow = false;
void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
void capture(void *, Uint8 *stream, int bytes)
{
	const auto samples = std::size_t(bytes) / 2;
	if (bytes < 0 || bytes % 4 || samples > captured.size() - captured_samples) {
		overflow = true;
		return;
	}
	std::memcpy(captured.data() + captured_samples, stream, std::size_t(bytes));
	captured_samples += samples;
}

void compare(const xl_compat::AudioSample &sample, int repetitions, int volume, int count)
{
	SDL_PauseAudio(1);
	Mix_HaltChannel(-1);
	captured.fill(0);
	captured_samples = 0;
	overflow = false;
	Mix_Chunk chunk{ 0, reinterpret_cast<Uint8 *>(const_cast<std::int16_t *>(sample.stereo.data())), Uint32(sample.stereo.size() * 2), 128 };
	for (int i = 0; i < count; ++i) {
		Mix_Volume(i, volume);
		require(Mix_SetPanning(i, 127, 127) != 0, "SDL reference panning failed");
		require(Mix_PlayChannel(i, &chunk, repetitions - 1) == i, "SDL reference voice failed to start");
	}
	SDL_PauseAudio(0);
	const auto started = SDL_GetTicks();
	const auto expected = sample.stereo.size() * repetitions;
	for (;;) {
		SDL_LockAudio();
		const bool done = captured_samples >= expected;
		SDL_UnlockAudio();
		if (done) break;
		require(SDL_GetTicks() - started < 5000, "SDL reference output timed out");
		SDL_Delay(1);
	}
	SDL_PauseAudio(1);
	require(!overflow && !Mix_Playing(-1), "SDL reference voice did not complete within its captured output");
	xl_compat::AudioVoices actual;
	actual.volume = volume;
	for (int i = 0; i < count; ++i) require(actual.start(0, repetitions, i) >= 0, "Native reference voice allocation failed");
	std::vector<std::int16_t> output(captured_samples);
	actual.mix(reinterpret_cast<unsigned char *>(output.data()), output.size() / 2, { sample });
	for (std::size_t i = 0; i < output.size(); ++i)
		if (output[i] != captured[i]) {
			std::cerr << "sample=" << i << " voices=" << count << " repeats=" << repetitions << " volume=" << volume
			          << " native=" << output[i] << " SDL=" << captured[i] << '\n';
			throw std::runtime_error("Native output differs from the actual SDL_mixer channel path");
		}
	require(actual.completions == unsigned(count), "Native output did not finish all reference voices");
}
} // namespace

int main()
{
	try {
		SDL_putenv(const_cast<char *>("SDL_AUDIODRIVER=dummy"));
		require(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, "Cannot initialize SDL reference output");
		require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 128) == 0, "Cannot open SDL reference mixer");
		SDL_PauseAudio(1);
		int rate, channels;
		Uint16 format;
		require(Mix_QuerySpec(&rate, &format, &channels) && rate == 44100 && format == AUDIO_S16SYS && channels == 2, "SDL reference format differs");
		require(Mix_AllocateChannels(4) >= 4, "Cannot allocate SDL reference channels");
		Mix_SetPostMix(capture, nullptr);
		xl_compat::AudioSample sample;
		sample.rate = 44100;
		const std::array<std::int16_t, 17> boundaries{ -32768, -32767, -25500, -1020, -511, -255, -3, -1, 0, 1, 3, 255, 511, 1020, 25500, 32766, 32767 };
		for (unsigned i = 0; i < 513; ++i) {
			sample.stereo.push_back(boundaries[i % boundaries.size()]);
			sample.stereo.push_back(boundaries[(i * 7 + 3) % boundaries.size()]);
		}
		unsigned cases = 0;
		for (int count : { 1, 2, 4 })
			for (int repeats : { 1, 2, 3 })
				for (int volume : { 0, 1, 20, 64, 127, 128 }) {
					compare(sample, repeats, volume, count);
					++cases;
				}
		Mix_CloseAudio();
		SDL_Quit();
		std::cout << "PASS: " << cases << " actual SDL_mixer output comparisons, stereo extremes, repeats, volume and clipping\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		Mix_CloseAudio();
		SDL_Quit();
		return 1;
	}
}
