/* Android sound provenance diagnostics, called only on the engine thread */
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

#ifdef __cplusplus
}
#endif
#endif
