#ifndef DXX_XL_NAMED_AUDIO_STATE_H
#define DXX_XL_NAMED_AUDIO_STATE_H
#include <nlohmann/json.hpp>
namespace xl_compat
{
nlohmann::json named_audio_snapshot();
bool restore_named_audio(const nlohmann::json &state, bool apply);
nlohmann::json named_audio_diagnostics();
} // namespace xl_compat
#endif
