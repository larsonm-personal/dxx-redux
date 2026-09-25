#ifndef INPUT_DEMO_AI_TRACE_H
#define INPUT_DEMO_AI_TRACE_H

#include <nlohmann/json_fwd.hpp>

// Named raw AI storage and runtime state, without simulation or RNG changes
// local_default is slot zero; omitted locals equal that complete record
// Explicit slots retain all fields, including zero values unlike the default
nlohmann::ordered_json input_demo_ai_trace_snapshot();
nlohmann::ordered_json input_demo_ai_local_trace_snapshot(int slot);

#endif
