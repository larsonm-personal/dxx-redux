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
void android_ogl_loading_probe_phase(const char *phase);
void android_ogl_loading_probe_frame(void);
void android_ogl_loading_background_test_begin(void);
const char *android_ogl_loading_background_test_result(void);
void android_ogl_graphics_debug_black_frame(void);
void android_ogl_menu_probe_request(void);
int android_ogl_menu_probe_active(void);
void android_ogl_menu_probe_source(const unsigned char *pixels, int width, int height, int stride,
                                   int x, int y, int screen_width, int screen_height,
                                   const unsigned char *palette);
void android_ogl_menu_probe_after_blit(void);
void android_ogl_menu_probe_before_swap(unsigned long long flip);
const char *android_ogl_menu_probe_result_json(void);
void android_ogl_scene_probe_request(int minimum_passes);
void android_ogl_scene_probe_discard_base_view(void);
void android_ogl_scene_probe_after_pass(int depth);
void android_ogl_scene_probe_before_swap(unsigned long long flip);
const char *android_ogl_scene_probe_result_json(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
