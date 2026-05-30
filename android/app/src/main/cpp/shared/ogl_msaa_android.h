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

#endif

#endif