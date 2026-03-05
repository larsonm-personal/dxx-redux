/*
 * SDL_androidaudio.c — SDL 1.2 audio driver for Android using OpenSL ES.
 *
 * The game mixes 8-bit unsigned PCM (AUDIO_U8, 22050 Hz, stereo, 1024 frames).
 * This driver converts U8→S16 and feeds double-buffered output to OpenSL ES.
 *
 * Follows the exact SDL 1.2 audio driver convention used by dummy, dsp, alsa etc:
 *   Available()  → can this driver work?
 *   CreateDevice()→ allocate SDL_AudioDevice + wire function pointers
 *   OpenAudio()  → init hardware, allocate buffers
 *   WaitAudio()  → block until device ready for more data
 *   PlayAudio()  → submit mixed buffer to device
 *   GetAudioBuf()→ return pointer to mix buffer
 *   CloseAudio() → shutdown, free resources
 */

#include "SDL_config.h"

#if SDL_AUDIO_DRIVER_ANDROID

#include <string.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "SDL_audiomem.h"
#include "SDL_audio_c.h"
#include "SDL_audiodev_c.h"
#include "SDL_androidaudio.h"

#define LOG_TAG "DXX-Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define ANDROIDAUD_DRIVER_NAME "android"

/* ── Diagnostic counters ───────────────────────────────────── */
#include <time.h>
static int      g_play_count        = 0;   /* total PlayAudio calls         */
static int      g_enqueue_fail      = 0;   /* Enqueue failures              */
static int      g_sem_zero_count    = 0;   /* times sem was 0 (had to wait) */
static long     g_play_max_wait_ns  = 0;   /* worst SemWait time            */
static int      g_audio_freq        = 0;   /* stored sample rate for stats  */
static int      g_audio_buf_frames  = 0;   /* samples per buffer            */

/* ── Forward declarations ──────────────────────────────────── */
static int  ANDROIDAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void ANDROIDAUD_WaitAudio(_THIS);
static void ANDROIDAUD_PlayAudio(_THIS);
static Uint8 *ANDROIDAUD_GetAudioBuf(_THIS);
static void ANDROIDAUD_CloseAudio(_THIS);

/* ── Bootstrap ─────────────────────────────────────────────── */

static int ANDROIDAUD_Available(void)
{
    /* Always available on Android — no env-var gating. */
    return 1;
}

static void ANDROIDAUD_DeleteDevice(SDL_AudioDevice *device)
{
    if (device) {
        SDL_free(device->hidden);
        SDL_free(device);
    }
}

static SDL_AudioDevice *ANDROIDAUD_CreateDevice(int devindex)
{
    SDL_AudioDevice *self;

    self = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
    if (self) {
        SDL_memset(self, 0, sizeof(*self));
        self->hidden = (struct SDL_PrivateAudioData *)
                       SDL_malloc(sizeof(*self->hidden));
    }
    if (!self || !self->hidden) {
        SDL_OutOfMemory();
        if (self) SDL_free(self);
        return NULL;
    }
    SDL_memset(self->hidden, 0, sizeof(*self->hidden));

    self->OpenAudio   = ANDROIDAUD_OpenAudio;
    self->WaitAudio   = ANDROIDAUD_WaitAudio;
    self->PlayAudio   = ANDROIDAUD_PlayAudio;
    self->GetAudioBuf = ANDROIDAUD_GetAudioBuf;
    self->CloseAudio  = ANDROIDAUD_CloseAudio;
    self->free        = ANDROIDAUD_DeleteDevice;

    return self;
}

AudioBootStrap ANDROIDAUD_bootstrap = {
    ANDROIDAUD_DRIVER_NAME, "Android OpenSL ES audio",
    ANDROIDAUD_Available, ANDROIDAUD_CreateDevice
};

/* ── OpenSL ES buffer-queue callback ───────────────────────── */

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    SDL_AudioDevice *audio = (SDL_AudioDevice *)context;
    /* A buffer has finished playing — signal the SDL audio thread. */
    SDL_SemPost(audio->hidden->sem);
}

/* ── Driver implementation ─────────────────────────────────── */

static int ANDROIDAUD_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
    struct SDL_PrivateAudioData *h = this->hidden;
    SLresult result;

    LOGI("OpenAudio: freq=%d fmt=0x%04X channels=%d samples=%d",
         spec->freq, spec->format, spec->channels, spec->samples);

    h->mixlen  = spec->size;      /* bytes per callback (S16 stereo) */
    h->playlen = spec->size;       /* OpenSL ES also uses S16 — same size */

    /* Allocate mix buffer (SDL fills this via the game's callback) */
    h->mixbuf = (Uint8 *)SDL_AllocAudioMem(h->mixlen);
    if (!h->mixbuf) {
        LOGE("Failed to allocate mixbuf (%u bytes)", h->mixlen);
        return -1;
    }
    SDL_memset(h->mixbuf, spec->silence, h->mixlen);

    /* Allocate S16 play buffers */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        h->playbuf[i] = (Sint16 *)SDL_AllocAudioMem(h->playlen);
        if (!h->playbuf[i]) {
            LOGE("Failed to allocate playbuf[%d]", i);
            return -1;
        }
        SDL_memset(h->playbuf[i], 0, h->playlen);
    }
    h->next_buf = 0;

    /* Semaphore: start at NUM_BUFFERS so the first N writes don't block */
    h->sem = SDL_CreateSemaphore(NUM_BUFFERS);
    if (!h->sem) {
        LOGE("Failed to create semaphore");
        return -1;
    }

    /* ── Create OpenSL ES engine ─────────────────────────── */
    result = slCreateEngine(&h->engineObject, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("slCreateEngine failed: %d", (int)result);
        return -1;
    }
    (*h->engineObject)->Realize(h->engineObject, SL_BOOLEAN_FALSE);
    (*h->engineObject)->GetInterface(h->engineObject, SL_IID_ENGINE, &h->engineEngine);

    /* ── Create output mix ───────────────────────────────── */
    result = (*h->engineEngine)->CreateOutputMix(h->engineEngine,
                 &h->outputMixObject, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("CreateOutputMix failed: %d", (int)result);
        return -1;
    }
    (*h->outputMixObject)->Realize(h->outputMixObject, SL_BOOLEAN_FALSE);

    /* ── Create buffer-queue audio player ────────────────── */
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, NUM_BUFFERS
    };

    /* 16-bit PCM — matches SDL mixer output format (AUDIO_S16) */
    SLDataFormat_PCM format_pcm = {
        SL_DATAFORMAT_PCM,
        (SLuint32)spec->channels,
        (SLuint32)(spec->freq * 1000),   /* milliHz */
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        (spec->channels == 2)
            ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
            : SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = { &loc_bufq, &format_pcm };

    SLDataLocator_OutputMix loc_outmix = {
        SL_DATALOCATOR_OUTPUTMIX, h->outputMixObject
    };
    SLDataSink audioSnk = { &loc_outmix, NULL };

    const SLInterfaceID ids[1] = { SL_IID_BUFFERQUEUE };
    const SLboolean     req[1] = { SL_BOOLEAN_TRUE };

    result = (*h->engineEngine)->CreateAudioPlayer(h->engineEngine,
                 &h->playerObject, &audioSrc, &audioSnk, 1, ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed: %d", (int)result);
        return -1;
    }
    (*h->playerObject)->Realize(h->playerObject, SL_BOOLEAN_FALSE);

    /* Get play and buffer-queue interfaces */
    (*h->playerObject)->GetInterface(h->playerObject, SL_IID_PLAY,
                                     &h->playerPlay);
    (*h->playerObject)->GetInterface(h->playerObject, SL_IID_BUFFERQUEUE,
                                     &h->playerBufferQueue);

    /* Register callback */
    (*h->playerBufferQueue)->RegisterCallback(h->playerBufferQueue,
                                              bqPlayerCallback, this);

    /* Start playback */
    (*h->playerPlay)->SetPlayState(h->playerPlay, SL_PLAYSTATE_PLAYING);

    LOGI("OpenSL ES audio ready: %d Hz, %d ch, %u samples/frame, "
         "mixbuf=%u bytes, playbuf=%u bytes, num_buffers=%d",
         spec->freq, spec->channels, spec->samples, h->mixlen, h->playlen,
         NUM_BUFFERS);

    g_audio_freq = spec->freq;
    g_audio_buf_frames = spec->samples;
    g_play_count = 0;
    g_enqueue_fail = 0;
    g_sem_zero_count = 0;
    g_play_max_wait_ns = 0;

    return 0;
}

static void ANDROIDAUD_WaitAudio(_THIS)
{
    /* No-op: synchronization happens in PlayAudio via SemWait before Enqueue.
     * SDL_RunAudio calls WaitAudio AFTER PlayAudio, but we need to block
     * BEFORE enqueueing to avoid SL_RESULT_BUFFER_INSUFFICIENT. */
}

static void ANDROIDAUD_PlayAudio(_THIS)
{
    struct SDL_PrivateAudioData *h = this->hidden;
    int buf_idx = h->next_buf;

    /* SDL mixer already outputs S16 stereo — same format OpenSL ES expects.
     * Just copy directly, no format conversion needed. */
    SDL_memcpy(h->playbuf[buf_idx], h->mixbuf, h->playlen);

    /* Check if semaphore is zero (= would block = all buffers in use) */
    struct timespec tw_start, tw_end;
    clock_gettime(CLOCK_MONOTONIC, &tw_start);
    int sem_val = SDL_SemValue(h->sem);
    if (sem_val == 0) g_sem_zero_count++;

    /* Wait for a free buffer slot before enqueueing */
    SDL_SemWait(h->sem);

    clock_gettime(CLOCK_MONOTONIC, &tw_end);
    long wait_ns = (tw_end.tv_sec - tw_start.tv_sec) * 1000000000L
                 + (tw_end.tv_nsec - tw_start.tv_nsec);
    if (wait_ns > g_play_max_wait_ns) g_play_max_wait_ns = wait_ns;

    /* Enqueue the S16 buffer */
    SLresult r = (*h->playerBufferQueue)->Enqueue(
        h->playerBufferQueue, h->playbuf[buf_idx], h->playlen);
    if (r != SL_RESULT_SUCCESS) {
        /* If enqueue fails, post the semaphore so we don't deadlock */
        g_enqueue_fail++;
        SDL_SemPost(h->sem);
    }

    h->next_buf = (buf_idx + 1) % NUM_BUFFERS;
    g_play_count++;

    /* Log stats every ~2 seconds */
    if (g_audio_buf_frames > 0 && g_audio_freq > 0) {
        int log_interval = (g_audio_freq / g_audio_buf_frames) * 2;
        if (log_interval < 1) log_interval = 1;
        if (g_play_count % log_interval == 0) {
            LOGI("audio stats: play=%d sem_waits=%d enq_fail=%d max_wait=%ldus "
                 "freq=%d buf=%d",
                 g_play_count, g_sem_zero_count, g_enqueue_fail,
                 g_play_max_wait_ns / 1000, g_audio_freq, g_audio_buf_frames);
        }
    }
}

static Uint8 *ANDROIDAUD_GetAudioBuf(_THIS)
{
    return this->hidden->mixbuf;
}

static void ANDROIDAUD_CloseAudio(_THIS)
{
    struct SDL_PrivateAudioData *h = this->hidden;

    LOGI("CloseAudio");

    /* Destroy OpenSL ES objects in reverse order */
    if (h->playerObject) {
        (*h->playerPlay)->SetPlayState(h->playerPlay, SL_PLAYSTATE_STOPPED);
        (*h->playerObject)->Destroy(h->playerObject);
        h->playerObject     = NULL;
        h->playerPlay       = NULL;
        h->playerBufferQueue = NULL;
    }
    if (h->outputMixObject) {
        (*h->outputMixObject)->Destroy(h->outputMixObject);
        h->outputMixObject = NULL;
    }
    if (h->engineObject) {
        (*h->engineObject)->Destroy(h->engineObject);
        h->engineObject = NULL;
        h->engineEngine = NULL;
    }

    /* Free buffers */
    if (h->mixbuf) {
        SDL_FreeAudioMem(h->mixbuf);
        h->mixbuf = NULL;
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (h->playbuf[i]) {
            SDL_FreeAudioMem(h->playbuf[i]);
            h->playbuf[i] = NULL;
        }
    }

    /* Free semaphore */
    if (h->sem) {
        SDL_DestroySemaphore(h->sem);
        h->sem = NULL;
    }
}

/* ── Diagnostic accessors (called from game_introspect.cpp) ──────────── */

int androidaud_get_play_count(void)      { return g_play_count; }
int androidaud_get_enqueue_fail(void)    { return g_enqueue_fail; }
int androidaud_get_sem_zero_count(void)  { return g_sem_zero_count; }
long androidaud_get_play_max_wait_ns(void) { return g_play_max_wait_ns; }
int androidaud_get_audio_freq(void)      { return g_audio_freq; }
int androidaud_get_audio_buf_frames(void) { return g_audio_buf_frames; }

#endif /* SDL_AUDIO_DRIVER_ANDROID */
