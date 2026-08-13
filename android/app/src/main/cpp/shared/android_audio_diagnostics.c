#ifdef ANDROID

#include <android/log.h>
#include <SDL_audio.h>
#include <SDL_mixer.h>

#include "android_audio_diagnostics.h"
#include "android_log.h"

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

static int androidaud_sfx_chunk_ms(const void *chunk_buffer,
                                   unsigned int chunk_length, int *lead_ms)
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
	if (!chunk_buffer || chunk_length == 0)
		return -1;
	if (!Mix_QuerySpec(&actual_freq, &actual_fmt, &actual_ch) ||
	    actual_freq <= 0 || actual_ch <= 0)
		return -1;
	bytes_per_sample = (actual_fmt & 0xff) / 8;
	if (bytes_per_sample != 2)
		return -1;
	bytes_per_frame = bytes_per_sample * actual_ch;
	frames = chunk_length / bytes_per_frame;
	samples = (const Sint16 *) chunk_buffer;
	for (frame = 0; frame < frames; frame++) {
		int ch;
		int audible = 0;
		for (ch = 0; ch < actual_ch; ch++) {
			int sample = samples[(frame * actual_ch) + ch];
			if (sample < 0)
				sample = -sample;
			if (sample > 64) {
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

void androidaud_log_mixer_open_failed(const char *error)
{
	__android_log_print(ANDROID_LOG_DEBUG, "digi_mixer",
	                    "ERROR: Couldn't open audio: %s", error);
}

void androidaud_log_mixer_init(int requested_rate, int mixer_buffer_frames)
{
	int actual_rate = 0;
	Uint16 actual_format = 0;
	int actual_channels = 0;

	Mix_QuerySpec(&actual_rate, &actual_format, &actual_channels);
	__android_log_print(
	    ANDROID_LOG_DEBUG, "digi_mixer",
	    "Mix_OpenAudio ok: requested=%d actual=%d fmt=0x%04X ch=%d buf=%d (native_rate=%d)",
	    requested_rate, actual_rate, actual_format, actual_channels,
	    mixer_buffer_frames, g_android_native_sample_rate);
	debug_log(DLOG_GAME,
	          "[audio] init: mixer_rate=%d actual_rate=%d fmt=0x%04X ch=%d buf_frames=%d native_buf_frames=%d initial_queue_buffers=%d perf_mode_result=%d native_rate=%d cb_overruns=%d",
	          requested_rate, actual_rate, actual_format, actual_channels,
	          mixer_buffer_frames, androidaud_get_native_buffer_frames(),
	          androidaud_get_initial_queued_buffers(), androidaud_get_perf_mode_result(),
	          g_android_native_sample_rate, androidaud_get_callback_overrun_count());
}

void androidaud_log_sfx_latency_probe(int *last_logged_probe_count)
{
	int probe_count;

	if (!last_logged_probe_count)
		return;
	probe_count = androidaud_get_sfx_probe_count();
	if (probe_count == *last_logged_probe_count)
		return;
	*last_logged_probe_count = probe_count;
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

void androidaud_log_mixer_sfx_start(int soundnum, int channel,
                                    const void *chunk_buffer, unsigned int chunk_length, int *start_log_count)
{
	int lead_ms = -1;
	int chunk_ms;

	androidaud_note_sfx_start(soundnum, channel);
	chunk_ms = androidaud_sfx_chunk_ms(chunk_buffer, chunk_length, &lead_ms);
	if (!start_log_count)
		return;
	(*start_log_count)++;
	if (*start_log_count <= 128 || (*start_log_count % 32) == 0)
		debug_log(DLOG_GAME,
		          "[audio] sfx start: seq=%d sound=%d channel=%d lead_ms=%d chunk_ms=%d mixer_buf_frames=%d native_buf_frames=%d",
		          *start_log_count, soundnum, channel, lead_ms, chunk_ms,
		          androidaud_get_audio_buf_frames(),
		          androidaud_get_native_buffer_frames());
}

#endif
