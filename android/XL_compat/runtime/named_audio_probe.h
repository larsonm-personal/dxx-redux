#ifndef DXX_XL_NAMED_AUDIO_PROBE_H
#define DXX_XL_NAMED_AUDIO_PROBE_H
#include <nlohmann/json.hpp>
const char *xl_named_audio_probe(const char *operation);
nlohmann::json xl_named_audio_probe_diagnostics();
#endif
