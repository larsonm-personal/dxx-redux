#ifndef OGL_MSAA_ANDROID_H
#define OGL_MSAA_ANDROID_H

#ifdef ANDROID

struct android_ogl_msaa_state {
	unsigned int fbo;
	unsigned int color_rbo;
	unsigned int depth_rbo;
	int w;
	int h;
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

#endif

#endif
