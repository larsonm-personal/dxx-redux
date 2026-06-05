/*
 * SDL_androidaudio.c — Callback-driven OpenSL ES audio driver for SDL 1.2.
 *
 * SDL 1.2's SDL_RunAudio thread is parked (via a patch in CMakeLists.txt).
 * Instead, OpenSL ES drives audio via buffer-queue callbacks: each time a
 * buffer finishes playing, bqPlayerCallback mixes new audio directly into
 * the freed buffer via SDL_mixer's callback and re-enqueues it.
 */

#include "SDL_config.h"

#if SDL_AUDIO_DRIVER_ANDROID

#include <android/log.h>
#include <string.h>
#include <time.h>
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
static int      g_play_count        = 0;   /* total bqPlayerCallback calls   */
static int      g_enqueue_fail      = 0;   /* Enqueue failures               */
static int      g_audio_freq        = 0;   /* stored sample rate for stats   */
static int      g_audio_buf_frames  = 0;   /* samples per buffer             */
static int      g_callback_last_us  = 0;
static int      g_callback_max_us   = 0;
static int      g_callback_overrun_count = 0;
static int      g_perf_mode_result  = -1;
static int      g_sfx_pending       = 0;
static int      g_sfx_start_ms      = 0;
static int      g_sfx_start_cb      = 0;
static int      g_sfx_soundnum      = -1;
static int      g_sfx_channel       = -1;
static int      g_sfx_last_delay_ms = -1;
static int      g_sfx_last_soundnum = -1;
static int      g_sfx_last_channel  = -1;
static int      g_sfx_last_cb_delta = -1;
static int      g_sfx_last_queue_delay_ms = -1;
static int      g_sfx_last_estimated_output_ms = -1;
static int      g_sfx_probe_count   = 0;

/* ── Native audio properties (set from JNI before audio init) ─ */
int g_android_native_sample_rate  = 0;   /* 0 = not yet queried */
int g_android_native_buffer_frames = 0;

/* ── Forward declarations ──────────────────────────────────── */
static int  ANDROIDAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void ANDROIDAUD_WaitAudio(_THIS);
static void ANDROIDAUD_PlayAudio(_THIS);
static Uint8 *ANDROIDAUD_GetAudioBuf(_THIS);
static void ANDROIDAUD_CloseAudio(_THIS);

void androidaud_note_sfx_start(int soundnum, int channel)
{
    g_sfx_soundnum = soundnum;
    g_sfx_channel = channel;
    g_sfx_start_cb = g_play_count;
    g_sfx_start_ms = (int)SDL_GetTicks();
    g_sfx_pending = 1;
}

static int androidaud_buffer_ms(void)
{
    if (g_audio_freq <= 0 || g_audio_buf_frames <= 0) {
        return -1;
    }
    return (g_audio_buf_frames * 1000 + g_audio_freq - 1) / g_audio_freq;
}

static int androidaud_queue_delay_ms(void)
{
    int buffer_ms = androidaud_buffer_ms();
    int queued_buffers = INITIAL_QUEUED_BUFFERS;

    if (buffer_ms < 0) {
        return -1;
    }
    if (queued_buffers < 1) {
        queued_buffers = 1;
    } else if (queued_buffers > NUM_BUFFERS) {
        queued_buffers = NUM_BUFFERS;
    }
    return (queued_buffers - 1) * buffer_ms;
}

static long long androidaud_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((long long)ts.tv_sec * 1000000LL) + (ts.tv_nsec / 1000);
}

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

/* ── OpenSL ES buffer-queue callback ──────────────────────── */

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    SDL_AudioDevice *audio = (SDL_AudioDevice *)context;
    struct SDL_PrivateAudioData *h = audio->hidden;
    int buf_idx = h->next_buf;
    Uint8 *buf = (Uint8 *)h->playbuf[buf_idx];
    long long callback_start_us = androidaud_now_us();

    /* Fill with silence */
    SDL_memset(buf, 0, h->playlen);

    /* Mix audio directly from SDL_mixer */
    if (!audio->paused && audio->spec.callback) {
        SDL_mutexP(audio->mixer_lock);
        audio->spec.callback(audio->spec.userdata, buf, h->playlen);
        SDL_mutexV(audio->mixer_lock);
    }

    /* Re-enqueue for playback */
    SLresult r = (*h->playerBufferQueue)->Enqueue(
        h->playerBufferQueue, buf, h->playlen);
    if (r != SL_RESULT_SUCCESS) g_enqueue_fail++;

    h->next_buf = (buf_idx + 1) % NUM_BUFFERS;
    g_play_count++;
    {
        int elapsed_us = (int)(androidaud_now_us() - callback_start_us);
        int buffer_us = 0;
        g_callback_last_us = elapsed_us;
        if (elapsed_us > g_callback_max_us) {
            g_callback_max_us = elapsed_us;
        }
        if (g_audio_freq > 0 && g_audio_buf_frames > 0) {
            buffer_us = (g_audio_buf_frames * 1000000) / g_audio_freq;
            if (elapsed_us > buffer_us) {
                g_callback_overrun_count++;
            }
        }
    }

    if (g_sfx_pending) {
        int now_ms = (int)SDL_GetTicks();
        g_sfx_last_delay_ms = now_ms - g_sfx_start_ms;
        g_sfx_last_soundnum = g_sfx_soundnum;
        g_sfx_last_channel = g_sfx_channel;
        g_sfx_last_cb_delta = g_play_count - g_sfx_start_cb;
        g_sfx_last_queue_delay_ms = androidaud_queue_delay_ms();
        g_sfx_last_estimated_output_ms = g_sfx_last_delay_ms;
        if (g_sfx_last_queue_delay_ms > 0) {
            g_sfx_last_estimated_output_ms += g_sfx_last_queue_delay_ms;
        }
        g_sfx_pending = 0;
        g_sfx_probe_count++;
        if (g_sfx_probe_count <= 64 || (g_sfx_probe_count % 32) == 0) {
            LOGI("sfx latency probe: sound=%d channel=%d delay_ms=%d queue_ms=%d est_app_ms=%d callbacks=%d buf_idx=%d play_count=%d",
                 g_sfx_last_soundnum, g_sfx_last_channel, g_sfx_last_delay_ms,
                 g_sfx_last_queue_delay_ms, g_sfx_last_estimated_output_ms,
                 g_sfx_last_cb_delta, buf_idx, g_play_count);
        }
    }

    /* Log stats every ~2 seconds */
    if (g_audio_buf_frames > 0 && g_audio_freq > 0) {
        int log_interval = (g_audio_freq / g_audio_buf_frames) * 2;
        if (log_interval < 1) log_interval = 1;
        if (g_play_count % log_interval == 0) {
            LOGI("audio stats: callbacks=%d enq_fail=%d freq=%d buf=%d",
                 g_play_count, g_enqueue_fail, g_audio_freq,
                 g_audio_buf_frames);
        }
    }
}

/* ── Driver implementation ─────────────────────────────────── */

static int ANDROIDAUD_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
    struct SDL_PrivateAudioData *h = this->hidden;
    SLresult result;

    LOGI("OpenAudio: freq=%d fmt=0x%04X channels=%d samples=%d",
         spec->freq, spec->format, spec->channels, spec->samples);

    h->mixlen  = spec->size;
    h->playlen = spec->size;

    /* Allocate mix buffer (needed by SDL_RunAudio even though it's parked) */
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
    g_audio_freq = spec->freq;
    g_audio_buf_frames = spec->samples;
    g_callback_last_us = 0;
    g_callback_max_us = 0;
    g_callback_overrun_count = 0;
    g_play_count = 0;
    g_enqueue_fail = 0;
    g_sfx_pending = 0;
    g_sfx_last_delay_ms = -1;
    g_sfx_last_soundnum = -1;
    g_sfx_last_channel = -1;
    g_sfx_last_cb_delta = -1;
    g_sfx_last_queue_delay_ms = -1;
    g_sfx_last_estimated_output_ms = -1;
    g_sfx_probe_count = 0;

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

    /* Request low latency performance mode when Android exposes it. */
    const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
    const SLboolean     req[2] = { SL_BOOLEAN_TRUE,    SL_BOOLEAN_FALSE };

    result = (*h->engineEngine)->CreateAudioPlayer(h->engineEngine,
                 &h->playerObject, &audioSrc, &audioSnk, 2, ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed: %d", (int)result);
        return -1;
    }
    /* Set latency performance mode before Realize (API 26+). */
    {
        SLAndroidConfigurationItf cfg;
        if ((*h->playerObject)->GetInterface(h->playerObject,
                SL_IID_ANDROIDCONFIGURATION, &cfg) == SL_RESULT_SUCCESS) {
            SLuint32 perfMode = SL_ANDROID_PERFORMANCE_LATENCY;
            g_perf_mode_result = (*cfg)->SetConfiguration(cfg,
                SL_ANDROID_KEY_PERFORMANCE_MODE,
                &perfMode, sizeof(perfMode));
            LOGI("OpenSL ES: requested LATENCY performance mode result=%d",
                 g_perf_mode_result);
        } else {
            g_perf_mode_result = -2;
            LOGI("OpenSL ES: Android configuration interface unavailable");
        }
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

    /* Pre-enqueue silence to start the callback chain.
     * As each buffer finishes, bqPlayerCallback fires, mixes real audio
     * into the freed buffer, and re-enqueues it. */
    int initial_buffers = INITIAL_QUEUED_BUFFERS;
    if (initial_buffers < 1) {
        initial_buffers = 1;
    } else if (initial_buffers > NUM_BUFFERS) {
        initial_buffers = NUM_BUFFERS;
    }
    for (int i = 0; i < initial_buffers; i++) {
        (*h->playerBufferQueue)->Enqueue(h->playerBufferQueue,
                                         h->playbuf[i], h->playlen);
    }

    LOGI("OpenSL ES audio ready (callback-driven): %d Hz, %d ch, "
         "%u samples/frame, playbuf=%u bytes, num_buffers=%d, initial_buffers=%d",
         spec->freq, spec->channels, spec->samples, h->playlen,
         NUM_BUFFERS, initial_buffers);

    return 0;
}

/* ── Stubs (SDL_RunAudio is parked; audio is callback-driven) ── */

static void ANDROIDAUD_WaitAudio(_THIS) { }

static void ANDROIDAUD_PlayAudio(_THIS) { }

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
}

/* ── Diagnostic accessors (called from game_introspect.cpp) ──────────── */

int androidaud_get_play_count(void)      { return g_play_count; }
int androidaud_get_enqueue_fail(void)    { return g_enqueue_fail; }
int androidaud_get_audio_freq(void)      { return g_audio_freq; }
int androidaud_get_audio_buf_frames(void) { return g_audio_buf_frames; }
int androidaud_get_callback_last_us(void) { return g_callback_last_us; }
int androidaud_get_callback_max_us(void) { return g_callback_max_us; }
int androidaud_get_callback_overrun_count(void) { return g_callback_overrun_count; }
int androidaud_get_native_buffer_frames(void) { return g_android_native_buffer_frames; }
int androidaud_get_perf_mode_result(void) { return g_perf_mode_result; }
int androidaud_get_sfx_last_delay_ms(void) { return g_sfx_last_delay_ms; }
int androidaud_get_sfx_last_soundnum(void) { return g_sfx_last_soundnum; }
int androidaud_get_sfx_last_channel(void) { return g_sfx_last_channel; }
int androidaud_get_sfx_last_cb_delta(void) { return g_sfx_last_cb_delta; }
int androidaud_get_sfx_last_queue_delay_ms(void) { return g_sfx_last_queue_delay_ms; }
int androidaud_get_sfx_last_estimated_output_ms(void) { return g_sfx_last_estimated_output_ms; }
int androidaud_get_sfx_probe_count(void) { return g_sfx_probe_count; }
int androidaud_get_initial_queued_buffers(void) { return INITIAL_QUEUED_BUFFERS; }

#endif /* SDL_AUDIO_DRIVER_ANDROID */
