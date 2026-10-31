#ifndef DXX_XL_RUNTIME_LIGHTNING_OBJECTS_H
#define DXX_XL_RUNTIME_LIGHTNING_OBJECTS_H
#ifdef __cplusplus
#include "../lightning_emitter.h"
#include "lightning_scene.h"
#include "../lightning_light.h"
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
void bind_lightning_objects(const Inventory &level);
void lightning_objects_clock(int milliseconds);
void lightning_objects_frame();
const LightningEmitter *lightning_object_emitter(int index);
const LightningRenderFrame *lightning_object_render_frame(int index, unsigned bolt);
std::vector<LightningSceneItem> lightning_scene_items(const std::vector<int> &visible, const std::vector<int> &rendered_objects, const LightningPoint &eye, int maximum_depth);
bool lightning_scene_item_valid(const LightningSceneItem &item);
std::map<int, LightningSegmentLight> lightning_object_lights();
nlohmann::json lightning_objects_snapshot();
bool restore_lightning_objects(const nlohmann::json &state, bool apply);
bool lightning_audio_matches(const nlohmann::json &audio);
} // namespace xl_compat
extern "C" {
#endif
void xl_lightning_objects_clear(void);
void xl_lightning_objects_mute(void);
void xl_lightning_objects_clock(void);
void xl_lightning_objects_frame(void);
int xl_lightning_object_set_enabled(int object, int enabled);
#ifdef __cplusplus
}
#endif
#endif
