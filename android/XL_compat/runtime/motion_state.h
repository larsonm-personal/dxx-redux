#ifndef DXX_XL_MOTION_STATE_H
#define DXX_XL_MOTION_STATE_H
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
// Pure observation; C gameplay headers remain independent of JSON and its ABI
nlohmann::json speed_boost_snapshot();
nlohmann::json spawn_points_snapshot();
nlohmann::json projectile_snapshot();
} // namespace xl_compat
#endif
