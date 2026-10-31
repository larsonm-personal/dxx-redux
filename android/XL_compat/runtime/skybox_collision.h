#ifndef DXX_XL_RUNTIME_SKYBOX_COLLISION_H
#define DXX_XL_RUNTIME_SKYBOX_COLLISION_H
#ifdef __cplusplus
extern "C" {
#endif
struct object;
struct fvi_info;
struct vms_vector;
int xl_skybox_wall_collision(struct object *, struct fvi_info *, const struct vms_vector *destination);
void xl_skybox_collision_clear(void);
#ifdef __cplusplus
}
#include "../inventory.h"
namespace xl_compat
{
void bind_skybox_collision(const Inventory &level);
}
#endif
#endif
