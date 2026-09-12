#ifndef DXX_GUIDEBOT_PATH_RECOVERY_H
#define DXX_GUIDEBOT_PATH_RECOVERY_H

struct object;
struct vms_vector;

#ifdef __cplusplus
extern "C" {
#endif

/* Adjust an unscaled steering command; true means precision must bypass smoothing */
int guidebot_route_steer_approach(const struct object *objp, const struct vms_vector *goal_point,
                                struct vms_vector *velocity);
void guidebot_route_recover_approach(struct object *objp, struct vms_vector *goal_point);

#ifdef __cplusplus
}
#endif

#endif
