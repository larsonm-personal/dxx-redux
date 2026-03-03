/*
 * SDL_androidaudio.h — Private data for the Android OpenSL ES audio driver.
 *
 * SDL 1.2 driver interface: see SDL_sysaudio.h for the function-pointer table.
 */
#ifndef _SDL_androidaudio_h
#define _SDL_androidaudio_h

#include "SDL_config.h"
#include "SDL_sysaudio.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SDL_mutex.h>

/* Hidden "this" pointer for the audio functions */
#define _THIS	SDL_AudioDevice *this

#define NUM_BUFFERS 2

struct SDL_PrivateAudioData {
    /* OpenSL ES objects */
    SLObjectItf  engineObject;
    SLEngineItf  engineEngine;
    SLObjectItf  outputMixObject;
    SLObjectItf  playerObject;
    SLPlayItf    playerPlay;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;

    /* Audio buffers — SDL mixes into mixbuf (U8), we convert to
     * playbuf (S16) before enqueueing to OpenSL ES. */
    Uint8   *mixbuf;              /* SDL mix buffer (spec.size bytes, U8) */
    Sint16  *playbuf[NUM_BUFFERS]; /* double-buffered S16 output */
    int      next_buf;            /* which playbuf to fill next */
    Uint32   mixlen;              /* spec.size (bytes of U8 data) */
    Uint32   playlen;             /* playbuf size in bytes (mixlen * 2 if U8→S16) */

    /* Synchronization */
    SDL_sem *sem;                 /* posted by OpenSL callback when buffer done */
};

#endif /* _SDL_androidaudio_h */
