#include <stdio.h>
#include <stdlib.h>

#include "ogl_gpu_timer_android.h"

static int ready[3];
static int pending[3];
static int active;
static int begins;
static int reads;
static int disjoint;

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "GPU timer check failed at line %d: %s\n", __LINE__, #condition); \
	exit(1); } } while (0)

void glGenQueries(GLsizei count, GLuint *queries)
{
	CHECK(count == 3);
	for (int i = 0; i < count; ++i)
		queries[i] = i + 1;
}

void glGetIntegerv(GLenum name, GLint *value)
{
	CHECK(name == 0x8FBB);
	*value = disjoint;
}

void glGetQueryObjectuiv(GLuint query, GLenum name, GLuint *value)
{
	CHECK(query >= 1 && query <= 3 && pending[query - 1]);
	if (name == GL_QUERY_RESULT_AVAILABLE) {
		*value = ready[query - 1];
	} else {
		CHECK(name == GL_QUERY_RESULT && ready[query - 1]);
		pending[query - 1] = 0;
		*value = query * 1000000;
		++reads;
	}
}

void glBeginQuery(GLenum target, GLuint query)
{
	CHECK(target == 0x88BF && !active && !pending[query - 1]);
	active = (int)query;
	ready[query - 1] = 0;
	++begins;
}

void glEndQuery(GLenum target)
{
	CHECK(target == 0x88BF && active);
	pending[active - 1] = 1;
	active = 0;
}

int main(void)
{
	GLuint queries[3] = { 0 };
	int write = 0, count = 0, in_flight = 0, gpu_us = 0;
	struct android_ogl_gpu_timer_state state = {
		queries, 3, &write, &count, &in_flight, &gpu_us
	};
	/* Several delayed GPU frames must fill the ring without forcing a result read */
	for (int frame = 0; frame < 12; ++frame) {
		android_ogl_gpu_timer_begin_frame(&state);
		android_ogl_gpu_timer_begin_frame(&state);
		android_ogl_gpu_timer_end_frame(&state);
	}
	CHECK(count == 3 && begins == 3 && reads == 0 && !in_flight);
	/* Retire completed queries in order, recycle their slots, and survive wraparound */
	for (int frame = 0; frame < 9; ++frame) {
		int oldest = (write - count + 3) % 3;
		ready[oldest] = 1;
		disjoint = frame == 8;
		int prior_gpu_us = gpu_us;
		android_ogl_gpu_timer_begin_frame(&state);
		CHECK(gpu_us == (disjoint ? prior_gpu_us : (oldest + 1) * 1000));
		CHECK(count == 2 && in_flight);
		android_ogl_gpu_timer_end_frame(&state);
		CHECK(count == 3 && !in_flight);
	}
	CHECK(reads == 9 && begins == 12);
	puts("PASS");
	return 0;
}
