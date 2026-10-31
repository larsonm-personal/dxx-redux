#ifndef DXX_XL_RUNTIME_LIGHTNING_RENDERER_H
#define DXX_XL_RUNTIME_LIGHTNING_RENDERER_H
namespace xl_compat
{
struct LightningSceneItem;
class ParticleTexture;
void draw_lightning_scene_item(const LightningSceneItem &item, ParticleTexture &image);
void finish_lightning_batch();
void discard_lightning_context(bool abandoned);
} // namespace xl_compat
#endif
