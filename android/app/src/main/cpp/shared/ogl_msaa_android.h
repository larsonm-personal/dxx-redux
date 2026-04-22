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

void android_ogl_msaa_destroy_fbo(struct android_ogl_msaa_state *state);
int android_ogl_msaa_create_fbo(struct android_ogl_msaa_state *state,
                                int max_samples,
                                int samples,
                                int w,
                                int h);

#endif

#endif