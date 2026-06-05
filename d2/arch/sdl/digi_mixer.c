/*
 * This is an alternate backend for the sound effect system.
 * It uses SDL_mixer to provide a more reliable playback,
 * and allow processing of multiple audio formats.
 *
 * This file is based on the original D1X arch/sdl/digi.c
 *
 *  -- MD2211 (2006-10-12)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_mixer.h>

#ifdef ANDROID
#include <android/log.h>
#include "android_log.h"
#define MIXLOG(...) __android_log_print(ANDROID_LOG_DEBUG, "digi_mixer", __VA_ARGS__)
extern void androidaud_note_sfx_start(int soundnum, int channel);
extern int androidaud_get_audio_buf_frames(void);
extern int androidaud_get_callback_max_us(void);
extern int androidaud_get_callback_overrun_count(void);
extern int androidaud_get_native_buffer_frames(void);
extern int androidaud_get_perf_mode_result(void);
extern int androidaud_get_sfx_last_delay_ms(void);
extern int androidaud_get_sfx_last_soundnum(void);
extern int androidaud_get_sfx_last_channel(void);
extern int androidaud_get_sfx_last_cb_delta(void);
extern int androidaud_get_sfx_last_queue_delay_ms(void);
extern int androidaud_get_sfx_last_estimated_output_ms(void);
extern int androidaud_get_sfx_probe_count(void);
extern int androidaud_get_initial_queued_buffers(void);
#else
#define MIXLOG(...) do {} while(0)
#endif

#include "pstypes.h"
#include "dxxerror.h"
#include "sounds.h"
#include "digi.h"
#include "digi_mixer.h"
#include "digi_mixer_music.h"
#include "console.h"
#include "config.h"
#include "args.h"

#include "fix.h"
#include "gr.h" // needed for piggy.h
#include "piggy.h"

#define MIX_DIGI_DEBUG 0
#define MIX_OUTPUT_FORMAT	AUDIO_S16
#define MIX_OUTPUT_CHANNELS	2
#define MAX_DIGI_SAMPLE_BYTES	(16 * 1024 * 1024)

#define MAX_SOUND_SLOTS 64
#ifdef ANDROID
#define SOUND_BUFFER_SIZE 256
extern int g_android_native_sample_rate;
#define DIGI_MIXER_OUTPUT_RATE (g_android_native_sample_rate > 0 ? g_android_native_sample_rate : SAMPLE_RATE_48K)
#else
#define SOUND_BUFFER_SIZE 512
#define DIGI_MIXER_OUTPUT_RATE SAMPLE_RATE_44K
#endif
#define MIN_VOLUME 10

static int digi_initialised = 0;
static int digi_max_channels = MAX_SOUND_SLOTS;
static inline int fix2byte(fix f) { return f < 0 ? 0 : f >= 65536 ? 255 : f / 256; }
Mix_Chunk SoundChunks[MAX_SOUNDS];
ubyte channels[MAX_SOUND_SLOTS];

#ifdef ANDROID
static int android_sfx_chunk_ms(const Mix_Chunk *chunk, int *lead_ms)
{
	int actual_freq = 0;
	Uint16 actual_fmt = 0;
	int actual_ch = 0;
	int bytes_per_sample;
	int bytes_per_frame;
	int frames;
	int lead_frames = 0;
	const Sint16 *samples;
	int frame;

	if (lead_ms)
		*lead_ms = -1;
	if (!chunk || !chunk->abuf || chunk->alen == 0)
		return -1;
	if (!Mix_QuerySpec(&actual_freq, &actual_fmt, &actual_ch) || actual_freq <= 0 || actual_ch <= 0)
		return -1;
	bytes_per_sample = (actual_fmt & 0xff) / 8;
	if (bytes_per_sample != 2)
		return -1;
	bytes_per_frame = bytes_per_sample * actual_ch;
	frames = chunk->alen / bytes_per_frame;
	samples = (const Sint16 *)chunk->abuf;
	for (frame = 0; frame < frames; frame++)
	{
		int ch;
		int audible = 0;
		for (ch = 0; ch < actual_ch; ch++)
		{
			int sample = samples[(frame * actual_ch) + ch];
			if (sample < 0)
				sample = -sample;
			if (sample > 64)
			{
				audible = 1;
				break;
			}
		}
		if (audible)
			break;
		lead_frames++;
	}
	if (lead_ms)
		*lead_ms = (lead_frames * 1000) / actual_freq;
	return (frames * 1000) / actual_freq;
}
#endif

#ifdef __linux__
static int digi_mixer_check_soundfont(const char *path, void *data)
{
	FILE *file = fopen(path, "r");
	if (!file)
		return 0;
	fclose(file);
	return 1;
}
#endif

/* Initialise audio */
int digi_mixer_init()
{
#ifdef ANDROID
	int actual_freq = 0;
	Uint16 actual_fmt = 0;
	int actual_ch = 0;
#endif
	if (MIX_DIGI_DEBUG) con_printf(CON_DEBUG,"digi_init %d (SDL_Mixer)\n", MAX_SOUNDS);
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) Error("SDL audio initialisation failed: %s.", SDL_GetError());

	#ifdef __linux__
	// Use the soundfont in the AppImage if no other sound font specified
	Mix_Init(0); // hack to set soundfont_paths on Debian patched SDL-mixer
	if (!Mix_EachSoundFont(digi_mixer_check_soundfont, NULL) && getenv("APPDIR"))
	{
		char soundfonts[PATH_MAX];
		snprintf(soundfonts, sizeof(soundfonts),
			"%s/usr/share/sounds/sf3/default-GM.sf3", getenv("APPDIR"));
		Mix_SetSoundFonts(soundfonts);
	}
	#endif

	if (Mix_OpenAudio(DIGI_MIXER_OUTPUT_RATE, MIX_OUTPUT_FORMAT, MIX_OUTPUT_CHANNELS, SOUND_BUFFER_SIZE))
	{
		//edited on 10/05/98 by Matt Mueller - should keep running, just with no sound.
		MIXLOG("ERROR: Couldn't open audio: %s", SDL_GetError());
		con_printf(CON_URGENT,"\nError: Couldn't open audio: %s\n", SDL_GetError());
		GameArg.SndNoSound = 1;
		return 1;
	}
	{
#ifndef ANDROID
		int actual_freq; Uint16 actual_fmt; int actual_ch;
#endif
		Mix_QuerySpec(&actual_freq, &actual_fmt, &actual_ch);
#ifdef ANDROID
		MIXLOG("Mix_OpenAudio ok: requested=%d actual=%d fmt=0x%04X ch=%d buf=%d (native_rate=%d)",
			DIGI_MIXER_OUTPUT_RATE, actual_freq, actual_fmt, actual_ch, SOUND_BUFFER_SIZE,
			g_android_native_sample_rate);
#else
		MIXLOG("Mix_OpenAudio ok: requested=%d actual=%d fmt=0x%04X ch=%d buf=%d",
			DIGI_MIXER_OUTPUT_RATE, actual_freq, actual_fmt, actual_ch, SOUND_BUFFER_SIZE);
#endif
	}

#ifdef ANDROID
	debug_log(DLOG_GAME,
	          "[audio] init: mixer_rate=%d actual_rate=%d fmt=0x%04X ch=%d buf_frames=%d native_buf_frames=%d initial_queue_buffers=%d perf_mode_result=%d native_rate=%d cb_overruns=%d",
	          DIGI_MIXER_OUTPUT_RATE, actual_freq, actual_fmt, actual_ch, SOUND_BUFFER_SIZE,
	          androidaud_get_native_buffer_frames(), androidaud_get_initial_queued_buffers(),
	          androidaud_get_perf_mode_result(), g_android_native_sample_rate,
	          androidaud_get_callback_overrun_count());
#endif

	digi_max_channels = Mix_AllocateChannels(digi_max_channels);
	memset(channels, 0, MAX_SOUND_SLOTS);
	Mix_Pause(0);

	digi_initialised = 1;

	digi_mixer_set_digi_volume( (GameCfg.DigiVolume*32768)/8 );

	return 0;
}

/* Shut down audio */
void digi_mixer_close() {
	if (MIX_DIGI_DEBUG) con_printf(CON_DEBUG,"digi_close (SDL_Mixer)\n");
	if (!digi_initialised) return;
	digi_initialised = 0;
	Mix_CloseAudio();
}

/* channel management */
int digi_mixer_find_channel()
{
	int i;
	for (i = 0; i < digi_max_channels; i++)
		if (channels[i] == 0)
			return i;
	return -1;
}

void digi_mixer_free_channel(int channel_num)
{
	channels[channel_num] = 0;
}

/*
 * Play-time conversion. Performs output conversion only once per sound effect used.
 * Once the sound sample has been converted, it is cached in SoundChunks[]
 */
void mixdigi_convert_sound(int i)
{
	SDL_AudioCVT cvt;
	Uint8 *data = GameSounds[i].data;
	Uint32 dlen = GameSounds[i].length;
	int out_freq;
	Uint16 out_format;
	int out_channels;

	if (SoundChunks[i].abuf) return; //proceed only if not converted yet
	if (!data || dlen == 0 || dlen > MAX_DIGI_SAMPLE_BYTES)
	{
		if (MIX_DIGI_DEBUG) con_printf(CON_DEBUG, "skipping invalid sound %d length %u\n", i, dlen);
		return;
	}
	if (!Mix_QuerySpec(&out_freq, &out_format, &out_channels)) return;

	{
		int cvt_ret;
		int src_rate = GameArg.SndDigiSampleRate;
		size_t converted_len;
		if (src_rate <= 0)
			src_rate = SAMPLE_RATE_22K;
		cvt_ret = SDL_BuildAudioCVT(&cvt, AUDIO_U8, 1, src_rate, out_format, out_channels, out_freq);
		if (cvt_ret < 0 || cvt.len_mult <= 0)
		{
			con_printf(CON_DEBUG, "conversion setup of %d failed\n", i);
			return;
		}

		converted_len = (size_t)dlen * (size_t)cvt.len_mult;
		cvt.buf = malloc(converted_len);
		if (!cvt.buf)
			return;
		cvt.len = dlen;
		memcpy(cvt.buf, data, dlen);
		if (SDL_ConvertAudio(&cvt))
		{
			free(cvt.buf);
			con_printf(CON_DEBUG,"conversion of %d failed\n", i);
			return;
		}

		SoundChunks[i].abuf = cvt.buf;
		SoundChunks[i].alen = cvt.len_cvt;
		SoundChunks[i].allocated = 1;
		SoundChunks[i].volume = 128; // Max volume = 128
	}
}

// Volume 0-F1_0
int digi_mixer_start_sound(short soundnum, fix volume, int pan, int looping, int loop_start, int loop_end, int soundobj)
{
	int mix_vol = fix2byte(fixmul(digi_volume, volume));
	int mix_pan = fix2byte(pan);
	int mix_loop = looping * -1;
	int channel;
#ifdef ANDROID
	static int last_logged_probe_count = 0;
	static int start_log_count = 0;
	int probe_count;
#endif

	if (!digi_initialised) return -1;
	Assert(GameSounds[soundnum].data != (void *)-1);

#ifdef ANDROID
	probe_count = androidaud_get_sfx_probe_count();
	if (probe_count != last_logged_probe_count) {
		last_logged_probe_count = probe_count;
		debug_log(DLOG_GAME,
		          "[audio] sfx latency: probe=%d sound=%d channel=%d delay_ms=%d queue_ms=%d est_app_ms=%d callbacks=%d mixer_buf_frames=%d native_buf_frames=%d cb_max_us=%d cb_overruns=%d",
		          probe_count, androidaud_get_sfx_last_soundnum(),
		          androidaud_get_sfx_last_channel(), androidaud_get_sfx_last_delay_ms(),
		          androidaud_get_sfx_last_queue_delay_ms(),
		          androidaud_get_sfx_last_estimated_output_ms(),
		          androidaud_get_sfx_last_cb_delta(), androidaud_get_audio_buf_frames(),
		          androidaud_get_native_buffer_frames(), androidaud_get_callback_max_us(),
		          androidaud_get_callback_overrun_count());
	}
#endif

	mixdigi_convert_sound(soundnum);
	if (!SoundChunks[soundnum].abuf || SoundChunks[soundnum].alen == 0)
		return -1;

	if (MIX_DIGI_DEBUG) con_printf(CON_DEBUG,"digi_start_sound %d, volume %d, pan %d (start=%d, end=%d)\n", soundnum, mix_vol, mix_pan, loop_start, loop_end);

	channel = digi_mixer_find_channel();
	if (channel == -1)
		return -1;

	Mix_PlayChannel(channel, &(SoundChunks[soundnum]), mix_loop);
	Mix_SetPanning(channel, 255-mix_pan, mix_pan);
	if (volume > F1_0)
		Mix_SetDistance(channel, 0);
	else
		Mix_SetDistance(channel, 255-mix_vol);
#ifdef ANDROID
	androidaud_note_sfx_start(soundnum, channel);
	{
		int lead_ms = -1;
		int chunk_ms = android_sfx_chunk_ms(&SoundChunks[soundnum], &lead_ms);
		start_log_count++;
		if (start_log_count <= 128 || (start_log_count % 32) == 0)
			debug_log(DLOG_GAME,
			          "[audio] sfx start: seq=%d sound=%d channel=%d lead_ms=%d chunk_ms=%d mixer_buf_frames=%d native_buf_frames=%d",
			          start_log_count, soundnum, channel, lead_ms, chunk_ms,
			          androidaud_get_audio_buf_frames(),
			          androidaud_get_native_buffer_frames());
	}
#endif
	channels[channel] = 1;
	Mix_ChannelFinished(digi_mixer_free_channel);

	return channel;
}

void digi_mixer_set_channel_volume(int channel, int volume)
{
	int mix_vol = fix2byte(volume);
	if (!digi_initialised) return;
	Mix_SetDistance(channel, 255-mix_vol);
}

void digi_mixer_set_channel_pan(int channel, int pan)
{
	int mix_pan = fix2byte(pan);
	Mix_SetPanning(channel, 255-mix_pan, mix_pan);
}

void digi_mixer_stop_sound(int channel) {
	if (!digi_initialised) return;
	if (MIX_DIGI_DEBUG) con_printf(CON_DEBUG,"digi_stop_sound %d\n", channel);
	Mix_HaltChannel(channel);
	channels[channel] = 0;
}

void digi_mixer_end_sound(int channel)
{
	digi_mixer_stop_sound(channel);
	channels[channel] = 0;
}

void digi_mixer_set_digi_volume( int dvolume )
{
	digi_volume = dvolume;
	if (!digi_initialised) return;
	Mix_Volume(-1, fix2byte(dvolume));
}

int digi_mixer_is_sound_playing(int soundno) { return 0; }
int digi_mixer_is_channel_playing(int channel) { return 0; }

void digi_mixer_reset() {}
void digi_mixer_stop_all_channels()
{
	Mix_HaltChannel(-1);
	memset(channels, 0, MAX_SOUND_SLOTS);
}

extern void digi_end_soundobj(int channel);

 //added on 980905 by adb to make sound channel setting work
void digi_mixer_set_max_channels(int n) { }
int digi_mixer_get_max_channels() { return digi_max_channels; }
// end edit by adb

#ifndef NDEBUG
void digi_mixer_debug() {}
#endif

void digi_mixer_free_cached_sounds()
{
	for (int i = 0; i < MAX_SOUNDS; i++)
		if (SoundChunks[i].allocated) {
			free(SoundChunks[i].abuf);
			SoundChunks[i].abuf = NULL;
			SoundChunks[i].allocated = 0;
		}
}
