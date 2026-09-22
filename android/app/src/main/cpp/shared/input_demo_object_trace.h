#ifndef INPUT_DEMO_OBJECT_TRACE_H
#define INPUT_DEMO_OBJECT_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Named live-object fields, segment links and allocator state; no simulation mutation */
int input_demo_object_trace_write(uint32_t frame, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif
#endif
