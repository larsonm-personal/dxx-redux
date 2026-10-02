#ifndef OGL_MSAA_PROBE_ANDROID_H
#define OGL_MSAA_PROBE_ANDROID_H

#ifdef ANDROID
#ifdef __cplusplus
extern "C" {
#endif

/* Opt-in game-thread diagnostic; never called by normal rendering */
void android_ogl_msaa_probe(int requested_samples, int logical_width, int logical_height);
const char *android_ogl_msaa_probe_result_json(void);
#ifdef INTROSPECT_ON
void android_ogl_graphics_debug_black_frame(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
