#ifndef DXX_ANDROID_GRAPHICS_SAFETY_H
#define DXX_ANDROID_GRAPHICS_SAFETY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int android_graphics_safety_initialize(const char *root);
void android_graphics_safety_shutdown(void);
void android_graphics_safety_first_run_marker(const char *path);
/* Game-thread completed main view, consumed by its EGL presentation */
void android_graphics_safety_main_view_rendered(void);
void android_graphics_safety_presented(int success, uint64_t generation);
int android_graphics_safety_preview_ready(uint64_t id);
int android_graphics_safety_preview_option(uint64_t id, const char *name, int value);
int android_graphics_safety_preview_done(uint64_t id);
int android_graphics_safety_queue_option(const char *name, int value, int persist, int debounce);
int android_graphics_safety_note_option(const char *name, int value);
int android_graphics_safety_before_mode(int width, int height);
void android_graphics_safety_event_tick(void);
int android_graphics_safety_before_main_view(void);
void android_graphics_safety_ui_state(int foreground, int blocked);
int android_graphics_safety_arm(uint64_t id);
int android_graphics_safety_decide(uint64_t id, int accept, const char *reason);
void android_graphics_safety_renderer_failed(const char *reason);
/* Game-thread mode restore uses lazy texture upload to meet the bounded watchdog */
int android_graphics_safety_restoring_mode(void);
void android_graphics_safety_state_json(char *buffer, size_t size);
#ifdef INTROSPECT_ON
/* Game-thread-only integration fault; consumed after the trial is armed */
void android_graphics_safety_debug_stall_once(int milliseconds);
void android_graphics_safety_debug_black_once(void);
int android_graphics_safety_debug_black_active(void);
void android_graphics_safety_debug_black_result(int valid);
#endif
void *android_graphics_safety_config_write_begin(int *ready);
int android_graphics_safety_config_write_value(const char *key, int fallback);
void android_graphics_safety_config_write_end(void *lock);
#ifdef __cplusplus
}
#endif
#endif
