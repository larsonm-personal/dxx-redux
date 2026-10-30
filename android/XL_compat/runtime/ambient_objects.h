#ifndef DXX_XL_RUNTIME_AMBIENT_OBJECTS_H
#define DXX_XL_RUNTIME_AMBIENT_OBJECTS_H
#ifdef __cplusplus
#include "../inventory.h"
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
void bind_ambient_objects(const Inventory &level);
nlohmann::json ambient_objects_snapshot();
bool restore_ambient_objects(const nlohmann::json &state, bool apply);
bool ambient_audio_matches(const nlohmann::json &audio);
} // namespace xl_compat
extern "C" {
#endif
void xl_ambient_objects_clear(void);
void xl_ambient_objects_start(void);
int xl_ambient_object_enabled(int object);
int xl_ambient_object_set_enabled(int object, int enabled);
int xl_ambient_object_noncolliding(int type);
#ifdef __cplusplus
}
#endif
#endif
