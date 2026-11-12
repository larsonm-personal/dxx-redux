#ifndef XL_SLOW_MOTION_SAVE_H
#define XL_SLOW_MOTION_SAVE_H
#include <nlohmann/json_fwd.hpp>
namespace xl_runtime
{
nlohmann::json weapon_clock_snapshot();
bool restore_weapon_clock(const nlohmann::json &saved, bool apply);
nlohmann::json slow_motion_snapshot();
nlohmann::json slow_motion_diagnostics();
nlohmann::json slow_motion_trace_snapshot();
bool restore_slow_motion(const nlohmann::json &saved, bool apply);
} // namespace xl_runtime
#endif
