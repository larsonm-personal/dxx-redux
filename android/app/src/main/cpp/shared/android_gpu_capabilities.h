#ifndef DXX_ANDROID_GPU_CAPABILITIES_H
#define DXX_ANDROID_GPU_CAPABILITIES_H

#ifdef __cplusplus
extern "C" {
#endif
/* Game-thread queries; reports are advisory to launcher UI, never settings */
void android_gpu_capabilities_set_root(const char *root);
void android_gpu_capabilities_query(float *max_anisotropy, int *max_samples, int *timer_available);
int android_gpu_msaa_samples(int requested);
unsigned int android_gpu_msaa_format(void);
int android_gpu_max_renderbuffer_size(void);
void android_gpu_capabilities_json(char *buffer, unsigned int size);
#ifdef INTROSPECT_ON
/* Negative capability test only; zero restores actual device capabilities */
void android_gpu_capabilities_debug_disable(int disabled);
#endif
#ifdef __cplusplus
}
#endif
#endif
