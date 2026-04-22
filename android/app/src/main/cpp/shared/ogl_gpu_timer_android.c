#ifdef ANDROID

#include "ogl_init.h"
#include "ogl_gpu_timer_android.h"

#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif

#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif

static void android_ogl_gpu_timer_store_result(
    const struct android_ogl_gpu_timer_state *state,
    GLuint ns)
{
	GLint disjoint = 0;

	if (!state || !state->gpu_time_us)
		return;

	glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
	if (!disjoint)
		*state->gpu_time_us = (int) (ns / 1000);
}

void android_ogl_gpu_timer_begin_frame(struct android_ogl_gpu_timer_state *state)
{
	int read_idx;

	if (!state || !state->queries || state->query_capacity <= 0 ||
	    !state->query_write || !state->query_count || !state->query_in_flight)
		return;

	if (!state->queries[0])
		glGenQueries(state->query_capacity, state->queries);

	if (*state->query_count > 0) {
		GLuint avail = 0;

		read_idx = (*state->query_write - *state->query_count + state->query_capacity) % state->query_capacity;
		glGetQueryObjectuiv(state->queries[read_idx], GL_QUERY_RESULT_AVAILABLE, &avail);
		if (avail) {
			GLuint ns = 0;

			glGetQueryObjectuiv(state->queries[read_idx], GL_QUERY_RESULT, &ns);
			android_ogl_gpu_timer_store_result(state, ns);
			(*state->query_count)--;
		} else if (*state->query_count >= state->query_capacity - 1) {
			GLuint ns = 0;

			glGetQueryObjectuiv(state->queries[read_idx], GL_QUERY_RESULT, &ns);
			android_ogl_gpu_timer_store_result(state, ns);
			(*state->query_count)--;
		}
	}

	if (*state->query_count < state->query_capacity) {
		glBeginQuery(GL_TIME_ELAPSED_EXT, state->queries[*state->query_write]);
		*state->query_in_flight = 1;
	}
}

void android_ogl_gpu_timer_end_frame(struct android_ogl_gpu_timer_state *state)
{
	if (!state || state->query_capacity <= 0 || !state->query_write ||
	    !state->query_count || !state->query_in_flight)
		return;
	if (!*state->query_in_flight)
		return;

	glEndQuery(GL_TIME_ELAPSED_EXT);
	*state->query_write = (*state->query_write + 1) % state->query_capacity;
	(*state->query_count)++;
	*state->query_in_flight = 0;
}

#endif