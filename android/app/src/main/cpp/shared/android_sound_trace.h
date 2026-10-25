/* Android sound provenance diagnostics; callback exceptions are documented below */
#ifndef ANDROID_SOUND_TRACE_H
#define ANDROID_SOUND_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void android_sound_trace_bank_open(const char *filename);
void android_sound_trace_asset_open(const char *role, const char *filename);
void android_sound_trace_loaded(int sample, const char *name, long long offset, int read_ok);
void android_sound_trace_level(void);
void android_sound_trace_converted(int sample, const void *data, unsigned int length,
                                   int source_rate, int output_rate, int output_format, int channels);
void android_sound_trace_play(int sample, const void *data, unsigned int length, int source_rate,
                              int channel);

#ifdef INTROSPECT_ON
#pragma pack(push, 8)
typedef struct android_sound_completion {
	int sample;
	unsigned int attempts, completed, stop_requested, failed;
	unsigned int last_elapsed_ms, last_bytes;
} android_sound_completion;
#pragma pack(pop)
/* Counts are process-cumulative D2 SDL_mixer diagnostics, not mission support flags
 * Engine-thread calls lock SDL audio; finished runs under the mixer's audio lock */
void android_sound_completion_begin(int channel, int sample, unsigned int bytes, int looping);
void android_sound_completion_stop(int channel);
void android_sound_completion_failed(int channel);
void android_sound_completion_finished(int channel);
int android_sound_completion_snapshot(android_sound_completion *output, int capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif
