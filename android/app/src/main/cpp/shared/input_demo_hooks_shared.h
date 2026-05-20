#ifndef INPUT_DEMO_HOOKS_SHARED_H
#define INPUT_DEMO_HOOKS_SHARED_H

#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "object.h"

void input_demo_capture_runtime_state_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_player_weapon_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_object_state_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
int input_demo_trace_collision_pose_active(void);
unsigned int input_demo_trace_collision_frame_index(void);
const char *input_demo_trace_collision_mode_name(void);
void input_demo_log_player_bump_probe(const char *step, object *obj0,
                                      object *obj1, const vms_vector *relative_velocity,
                                      const vms_vector *float_force, fix scale_num, fix scale_den,
                                      int damage_flag);

#endif
