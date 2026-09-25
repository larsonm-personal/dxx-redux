#ifndef INPUT_DEMO_WORLD_TRACE_H
#define INPUT_DEMO_WORLD_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Full boundary records and lossless per-frame world deltas, without mutation */
int input_demo_world_trace_write(uint32_t frame, const char *phase, char *error, size_t error_size);
void input_demo_trace_boundary(const char *phase);

#ifdef __cplusplus
}

#include <nlohmann/json_fwd.hpp>
nlohmann::ordered_json input_demo_endlevel_trace_snapshot();
#endif
#endif
