#ifndef OGL_GPU_TIMER_ANDROID_H
#define OGL_GPU_TIMER_ANDROID_H

#ifdef ANDROID

#include "ogl_init.h"

struct android_ogl_gpu_timer_state {
	GLuint *queries;
	int query_capacity;
	int *query_write;
	int *query_count;
	int *query_in_flight;
	int *gpu_time_us;
};

void android_ogl_gpu_timer_begin_frame(struct android_ogl_gpu_timer_state *state);
void android_ogl_gpu_timer_end_frame(struct android_ogl_gpu_timer_state *state);

#endif

#endif