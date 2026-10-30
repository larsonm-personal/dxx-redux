#ifndef DXX_XL_ROBOT_DISARM_STATE_H
#define DXX_XL_ROBOT_DISARM_STATE_H
#include <nlohmann/json.hpp>
namespace xl_compat
{
nlohmann::json disarm_snapshot();
bool disarm_restore(const nlohmann::json &state, bool apply);
} // namespace xl_compat
#endif
