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
	unsigned long long generation;
	unsigned long long bound_frame_count;
	unsigned long long resolve_count;
	unsigned long long active_frame_serial;
	unsigned long long last_resolved_frame_serial;
};

struct android_ogl_msaa_diagnostics {
	int effective_samples;
	int width;
	int height;
	int create_complete;
	int last_frame_resolved;
	unsigned int last_create_status;
	unsigned int last_gl_error;
	unsigned long long generation;
	unsigned long long bound_frame_count;
	unsigned long long resolve_count;
};

typedef void (*android_ogl_msaa_log_message_fn)(const char *message, void *user_data);

void android_ogl_msaa_destroy_fbo(struct android_ogl_msaa_state *state, int *bound);
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
