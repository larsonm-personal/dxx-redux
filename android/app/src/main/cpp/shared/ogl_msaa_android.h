#ifndef OGL_MSAA_ANDROID_H
#define OGL_MSAA_ANDROID_H

#ifdef ANDROID

struct android_ogl_msaa_state {
	unsigned int fbo;
	unsigned int color_rbo;
	unsigned int depth_rbo;
	int w;
	int h;
	int effective_samples;
	unsigned int last_create_status;
	unsigned int last_gl_error;
	unsigned int last_scene_gl_error;
	unsigned long long scene_error_count;
	unsigned long long generation;
	unsigned long long bound_frame_count;
	unsigned long long resolve_count;
	unsigned long long active_frame_serial;
	unsigned long long last_resolved_frame_serial;
	int failure_latched;
	int trace_remaining;
	unsigned long long resolve_failures;
	unsigned long long flip_serial;
	unsigned int flip_event;
};

struct android_ogl_msaa_diagnostics {
	int effective_samples;
	int width;
	int height;
	int create_complete;
	int last_frame_resolved;
	unsigned int last_create_status;
	unsigned int last_gl_error;
	unsigned int last_scene_gl_error;
	unsigned long long scene_error_count;
	unsigned long long generation;
	unsigned long long bound_frame_count;
	unsigned long long resolve_count;
	int failure_latched;
	unsigned long long resolve_failures;
	unsigned long long flip_serial;
	int trace_remaining;
};

typedef void (*android_ogl_msaa_log_message_fn)(const char *message, void *user_data);

/* Production and diagnostic resolves must use the actual window channel sizes */
unsigned int android_ogl_msaa_color_format(int red, int green, int blue, int alpha);
unsigned int android_ogl_msaa_capture_errors(const char *stage);
void android_ogl_msaa_capture_scene_errors(struct android_ogl_msaa_state *state);
#ifdef INTROSPECT_ON
/* Game-thread-only fault in the next actual color renderbuffer allocation */
void android_ogl_msaa_debug_fail_color_allocation_once(void);
#endif
void android_ogl_msaa_trace_stage(struct android_ogl_msaa_state *state, const char *stage, int bound, int depth);
void android_ogl_msaa_presented(struct android_ogl_msaa_state *state);

void android_ogl_msaa_destroy_fbo(struct android_ogl_msaa_state *state, int *bound);
/* Forget retired-context object names without deleting objects in the new context */
void android_ogl_msaa_forget_context(struct android_ogl_msaa_state *state, int *bound, int *frame_depth);
int android_ogl_msaa_create_fbo(struct android_ogl_msaa_state *state,
                                int *bound,
                                int max_samples,
                                int samples,
                                int w,
                                int h,
                                android_ogl_msaa_log_message_fn log_message,
                                void *log_user_data);
int android_ogl_msaa_begin_frame(struct android_ogl_msaa_state *state,
                                 int *bound, int *frame_depth,
                                 int max_samples, int samples, int w, int h,
                                 android_ogl_msaa_log_message_fn log_message,
                                 void *log_user_data);
void android_ogl_msaa_end_frame(int *frame_depth);
int android_ogl_msaa_resolve(struct android_ogl_msaa_state *state,
                             int *bound, int frame_depth, int w, int h);
void android_ogl_msaa_bind_window_backing(
    const struct android_ogl_msaa_state *state, int bound);
void android_ogl_msaa_bind_overlay_target(
    const struct android_ogl_msaa_state *state, int bound);
void android_ogl_msaa_get_diagnostics(
    const struct android_ogl_msaa_state *state,
    struct android_ogl_msaa_diagnostics *diagnostics);

#endif

#endif
