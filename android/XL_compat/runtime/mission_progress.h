#ifndef DXX_XL_MISSION_PROGRESS_H
#define DXX_XL_MISSION_PROGRESS_H
#ifdef __cplusplus
extern "C" {
#endif
void xl_mission_progress_new_game(void);
int xl_mission_progress_state(int level);
void xl_mission_progress_cache_restore(int enabled);
int xl_mission_progress_active(void);
int xl_mission_progress_prepare_advance(void);
int xl_mission_progress_reenter(void);
void xl_mission_progress_find_exit(void);
const char *xl_mission_progress_error(void);
#ifdef __cplusplus
}
void xl_mission_progress_bind(int ordinal);
void xl_mission_progress_mark(int level, int state);
#include <nlohmann/json.hpp>
namespace xl_compat
{
nlohmann::json mission_progress_snapshot();
bool restore_mission_progress(const nlohmann::json &saved, bool apply);
void mission_progress_native_controls();
} // namespace xl_compat
#endif
#endif
