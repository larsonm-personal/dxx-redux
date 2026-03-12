/*
 * SDL_androidaudio.h -- Private data for the Android OpenSL ES audio driver.
 */
#ifndef _SDL_androidaudio_h
#define _SDL_androidaudio_h

#include "SDL_config.h"
#include "SDL_sysaudio.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

/* Hidden "this" pointer for the audio functions */
#define _THIS	SDL_AudioDevice *this

#define NUM_BUFFERS 4

struct SDL_PrivateAudioData {
    /* OpenSL ES objects */
    SLObjectItf  engineObject;
    SLEngineItf  engineEngine;
    SLObjectItf  outputMixObject;
    SLObjectItf  playerObject;
    SLPlayItf    playerPlay;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;

    /* Audio buffers -- bqPlayerCallback mixes directly into playbuf
     * via SDL_mixer's callback, then re-enqueues.  mixbuf is kept
     * for SDL_RunAudio compatibility but unused in callback mode. */
    Uint8   *mixbuf;              /* SDL mix buffer (unused in callback mode) */
    Sint16  *playbuf[NUM_BUFFERS]; /* play buffers rotated by callback */
    int      next_buf;            /* which playbuf to fill next */
    Uint32   mixlen;              /* spec.size (bytes) */
    Uint32   playlen;             /* playbuf size in bytes */
};

#endif /* _SDL_androidaudio_h */
