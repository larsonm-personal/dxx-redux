#ifndef DXX_XL_CAMERA_COLLISION_H
#define DXX_XL_CAMERA_COLLISION_H
#ifdef __cplusplus
extern "C" {
#endif
struct object;
struct vms_vector;
/* -1 delegates non-camera pairs to the stock collision table */
int xl_camera_pair_collision(int a, int b);
int xl_camera_passive_contact(struct object *a, struct object *b);
int xl_camera_player_contact(struct object *a, struct object *b, struct vms_vector *point);
int xl_camera_weapon_contact(struct object *a, struct object *b, struct vms_vector *point);
void xl_camera_hit_state_clear(void);
unsigned xl_camera_player_weapon_contacts(void);
void xl_camera_hit_state_created(int owner);
void xl_camera_physics_unstick(struct object *actor, const struct object *camera,
                               const struct vms_vector *old_position, const struct vms_vector *attempted_position,
                               const struct vms_vector *offset, const struct vms_vector *old_velocity);
#ifdef __cplusplus
}
#include <cstdint>
#include <vector>
void xl_camera_validate_weapon_definitions();
namespace xl_compat
{
struct CameraHitState;
}
bool xl_camera_hit_state_get(int owner, xl_compat::CameraHitState &state);
struct XlCameraHitSave {
	int owner, signature, type;
	int32_t elapsed;
};
std::vector<XlCameraHitSave> xl_camera_hit_snapshot();
bool xl_camera_hit_restore(const std::vector<XlCameraHitSave> &saved, bool apply);
#endif
#endif
