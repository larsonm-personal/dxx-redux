#ifndef DXX_XL_SEISMIC_GAMEPLAY_H
#define DXX_XL_SEISMIC_GAMEPLAY_H
#ifdef __cplusplus
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
nlohmann::json seismic_gameplay_snapshot();
bool restore_seismic_gameplay(const nlohmann::json &state, bool apply);
} // namespace xl_compat
extern "C" {
#endif
struct trigger;
struct vms_vector;
void xl_seismic_clear(void);
void xl_seismic_bind(void);
int xl_seismic_owned(void);
int xl_seismic_action_valid(const struct trigger *action);
int xl_seismic_action(const struct trigger *action);
int xl_seismic_frame(void);
int xl_seismic_detonation(const struct vms_vector *position);
#ifdef __cplusplus
}
#endif
#endif
