// Actual native SDL_mixer output, using synthetic PCM rather than private music.
#include <cmath>
extern "C" {
#include "digi_mixer_music.h"
}
namespace music_pcm_fixture {
struct observation {
	unsigned frames = 0, crossings = 0;
	uint64_t absolute = 0;
	int previous = 0;
};
static observation observed;
static void postmix(void *, Uint8 *stream, int length)
{
	const auto *samples = reinterpret_cast<const Sint16 *>(stream);
	for (int i = 0; i < length / static_cast<int>(sizeof(Sint16)); i += 2) {
		const int sample = samples[i];
		observed.absolute += std::abs(sample);
		if (observed.previous <= 0 && sample > 0) ++observed.crossings;
		observed.previous = sample;
		++observed.frames;
	}
}
static observation measure()
{
	// Discard queued output from the preceding state, then observe real callbacks.
	SDL_Delay(100);
	SDL_LockAudio();
	observed = {};
	SDL_UnlockAudio();
	SDL_Delay(400);
	SDL_LockAudio();
	const observation result = observed;
	SDL_UnlockAudio();
	require(result.frames >= 22050 / 5, "music postmix callback made nonvacuous progress");
	return result;
}
static bool tone(const observation &o, int frequency, double gain)
{
	if (!o.frames) return false;
	const double hz = o.crossings * 22050.0 / o.frames;
	const double mean = static_cast<double>(o.absolute) / o.frames;
	const double expected = 12000.0 * 2.0 / 3.141592653589793 * gain;
	return std::abs(hz - frequency) < 8 && std::abs(mean - expected) < expected * 0.08;
}
static void write_tone(const char *path, int frequency)
{
	bytes wav = { 'R', 'I', 'F', 'F' };
	append_int(wav, 36 + 22050 * 4);
	append(wav, { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
	append_int(wav, 16);
	append(wav, { 1, 0, 2, 0 });
	append_int(wav, 22050);
	append_int(wav, 22050 * 4);
	append(wav, { 4, 0, 16, 0, 'd', 'a', 't', 'a' });
	append_int(wav, 22050 * 4);
	for (int i = 0; i < 22050; ++i) {
		const auto value = static_cast<uint16_t>(static_cast<int16_t>(12000 * std::sin(i * frequency * 2.0 * 3.141592653589793 / 22050)));
		for (int channel = 0; channel < 2; ++channel) append(wav, { static_cast<ubyte>(value), static_cast<ubyte>(value >> 8) });
	}
	write_fixture(path, wav);
}
static void run()
{
	static char driver[] = "SDL_AUDIODRIVER=dummy";
	require(SDL_putenv(driver) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) == 0, "initialize native music fixture");
	require(Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 512) == 0, "open actual native music mixer");
	int rate, channels;
	Uint16 format;
	require(Mix_QuerySpec(&rate, &format, &channels) && rate == 22050 && channels == 2 && format == AUDIO_S16SYS, "known postmix format");
	write_tone("music-a.wav", 440);
	write_tone("music-b.wav", 660);
	Mix_SetPostMix(postmix, nullptr);
	char a[] = "music-a.wav", b[] = "music-b.wav";
	mix_set_music_volume(8);
	require(mix_play_file(a, 1, nullptr) == 1 && Mix_PlayingMusic(), "native source A starts");
	const auto full = measure();
	require(tone(full, 440, 1), "source A actually emits expected PCM");
	require(!tone(full, 660, 1) && !tone(full, 440, 0.5) && full.absolute != 0, "wrong source, gain and silence controls reject audible PCM");
	mix_set_music_volume(4);
	const auto half = measure();
	require(tone(half, 440, 0.5) && !tone(half, 440, 1), "native music volume changes actual gain");
	mix_set_music_volume(0);
	const auto muted = measure();
	require(Mix_PlayingMusic() && muted.absolute == 0 && !tone(muted, 440, 1), "playing-but-muted produces silence, not an audible pass");
	mix_set_music_volume(8);
	require(tone(measure(), 440, 1), "unmute restores actual source A output");
	require(mix_play_file(b, 1, nullptr) == 1, "native source B starts");
	require(tone(measure(), 660, 1), "replacement source B is actually decoded");
	require(mix_play_file(a, 1, nullptr) == 1, "native source A restarts");
	require(tone(measure(), 440, 1), "A-B-A restores actual source output");
	mix_stop_music();
	require(!Mix_PlayingMusic() && measure().absolute == 0, "native stop does not leak PCM");
	mix_free_music();
	require(measure().absolute == 0, "native teardown retains silent callback output");
	Mix_SetPostMix(nullptr, nullptr);
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	require(PHYSFS_delete(a) && PHYSFS_delete(b), "remove synthetic music files");
	std::puts("PASS: native music PCM gain mute stop and source restoration");
}
}
