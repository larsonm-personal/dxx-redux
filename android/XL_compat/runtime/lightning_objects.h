#ifndef DXX_XL_RUNTIME_LIGHTNING_OBJECTS_H
#define DXX_XL_RUNTIME_LIGHTNING_OBJECTS_H
#ifdef __cplusplus
#include "../lightning_emitter.h"
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
void bind_lightning_objects(const Inventory &level);
void lightning_objects_clock(int milliseconds);
void lightning_objects_frame();
const LightningEmitter *lightning_object_emitter(int index);
nlohmann::json lightning_objects_snapshot();
bool restore_lightning_objects(const nlohmann::json &state, bool apply);
} // namespace xl_compat
extern "C" {
#endif
void xl_lightning_objects_clear(void);
void xl_lightning_objects_clock(void);
void xl_lightning_objects_frame(void);
int xl_lightning_object_set_enabled(int object, int enabled);
#ifdef __cplusplus
}
#endif
#endif
