#ifndef ANDROID_AUDIO_DIAGNOSTICS_H
#define ANDROID_AUDIO_DIAGNOSTICS_H

#ifdef ANDROID

extern int g_android_native_sample_rate;

void androidaud_log_mixer_init(int requested_rate, int actual_rate,
	unsigned int actual_format, int actual_channels, int mixer_buffer_frames);
void androidaud_log_sfx_latency_probe(int *last_logged_probe_count);
void androidaud_log_mixer_sfx_start(int soundnum, int channel,
	const void *chunk_buffer, unsigned int chunk_length, int *start_log_count);

#endif

#endif
