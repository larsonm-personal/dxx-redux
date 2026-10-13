#ifndef DXX_HEADLESS_AUDIO_H
#define DXX_HEADLESS_AUDIO_H

#include <stdlib.h>
#include <SDL.h>

extern "C" {
#include "args.h"
#include "digi.h"
#include "songs.h"
}

static void close_headless_audio(void)
{
	// Join the audio callback before engine allocations and SDL libraries retire
	digi_close();
	SDL_Quit();
}

static int init_headless_audio(void)
{
	static char sdl_audio_driver[] = "SDL_AUDIODRIVER=dummy";

	GameArg.SndNoMusic = 1;
	SDL_putenv(sdl_audio_driver);
	digi_select_system(GameArg.SndDisableSdlMixer ? SDLAUDIO_SYSTEM : SDLMIXER_SYSTEM);
	if (digi_init()) {
		close_headless_audio();
		return 0;
	}
	// Cover replay-load failures as well as successful completion
	if (atexit(close_headless_audio)) {
		close_headless_audio();
		return 0;
	}
	digi_set_digi_volume(0);
	songs_set_volume(0);
	return 1;
}

#endif
