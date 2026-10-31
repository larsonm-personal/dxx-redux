#ifndef DXX_XL_RUNTIME_PARTICLE_STATE_H
#define DXX_XL_RUNTIME_PARTICLE_STATE_H
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
nlohmann::json particle_objects_snapshot();
bool restore_particle_objects(const nlohmann::json &state, bool apply);
} // namespace xl_compat
#endif
