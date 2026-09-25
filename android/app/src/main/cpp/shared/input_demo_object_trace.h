#ifndef INPUT_DEMO_OBJECT_TRACE_H
#define INPUT_DEMO_OBJECT_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Named live-object fields, segment links and allocator state; no simulation mutation */
int input_demo_object_trace_write(uint32_t frame, char *error, size_t error_size);
int input_demo_object_trace_boundary(uint32_t frame, const char *phase, char *error, size_t error_size);

#ifdef __cplusplus
}

#include <nlohmann/json_fwd.hpp>
struct object;
// The caller supplies the engine slot for actors with slot-owned AI storage
nlohmann::ordered_json input_demo_object_trace_snapshot(const object &value, int slot);
#endif
#endif
