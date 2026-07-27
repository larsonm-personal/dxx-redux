#ifndef DXX_REDUX_GLES3_SHIM_ARRAY_SOURCES_H
#define DXX_REDUX_GLES3_SHIM_ARRAY_SOURCES_H

#include <stddef.h>

struct gles3_shim_array_source {
	int active;
	unsigned int buffer;
};

enum gles3_shim_array_source_kind {
	GLES3_SHIM_ARRAY_SOURCE_REJECT = -1,
	GLES3_SHIM_ARRAY_SOURCE_CLIENT = 0,
	GLES3_SHIM_ARRAY_SOURCE_BUFFER = 1
};

static inline int gles3_shim_choose_array_source(
    const struct gles3_shim_array_source *sources, size_t count,
    unsigned int *draw_buffer)
{
	unsigned int selected_buffer = 0;
	int has_client_array = 0;

	for (size_t i = 0; i < count; ++i) {
		if (!sources[i].active)
			continue;
		if (!sources[i].buffer) {
			has_client_array = 1;
			continue;
		}
		if (selected_buffer && selected_buffer != sources[i].buffer)
			return GLES3_SHIM_ARRAY_SOURCE_REJECT;
		selected_buffer = sources[i].buffer;
	}
	if (selected_buffer && has_client_array)
		return GLES3_SHIM_ARRAY_SOURCE_REJECT;
	*draw_buffer = selected_buffer;
	return selected_buffer ? GLES3_SHIM_ARRAY_SOURCE_BUFFER : GLES3_SHIM_ARRAY_SOURCE_CLIENT;
}

#endif
