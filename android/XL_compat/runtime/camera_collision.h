#ifndef DXX_XL_CAMERA_COLLISION_H
#define DXX_XL_CAMERA_COLLISION_H
#ifdef __cplusplus
extern "C" {
#endif
struct object;
struct vms_vector;
/* -1 delegates non-camera pairs to the stock collision table */
int xl_camera_pair_collision(int a, int b);
int xl_static_robot(const struct object *actor);
void xl_static_robot_restore_prepare(void);
void xl_static_robot_restore_payloads(void);
void xl_static_robot_restore_clear(void);
int xl_static_robot_bump(struct object *a, struct object *b);
int xl_static_robot_player_contact(struct object *robot, struct object *player);
int xl_static_robot_gauss(struct object *robot);
int xl_static_robot_query(const struct object *target, const struct object *moving,
                          const struct vms_vector *start, const struct vms_vector *end, int radius,
                          struct vms_vector *hit, struct vms_vector *normal);
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
bool xl_camera_contact_projectile(int id);
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
