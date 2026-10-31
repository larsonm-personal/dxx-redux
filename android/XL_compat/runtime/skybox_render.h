#ifndef DXX_XL_RUNTIME_SKYBOX_RENDER_H
#define DXX_XL_RUNTIME_SKYBOX_RENDER_H
#ifdef __cplusplus
extern "C" {
#endif
int xl_skybox_segment(int segment);
int xl_skybox_rendering(void);
int xl_skybox_object_pass(void);
void xl_skybox_render_objects(int window);
int xl_skybox_collect_view_objects(short *objects, int capacity);
void xl_skybox_render_scene(void);
void xl_skybox_render_clear(void);
#ifdef __cplusplus
}
#include "../inventory.h"
namespace xl_compat
{
void bind_skybox_render(const Inventory &level);
// Existing native/Android owner controls; does not change normal admission
void skybox_render_native_controls(const Inventory &level);
} // namespace xl_compat
#endif
#endif
