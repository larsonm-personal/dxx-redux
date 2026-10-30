#ifndef DXX_XL_ROBOT_DISARM_H
#define DXX_XL_ROBOT_DISARM_H
#ifdef __cplusplus
extern "C" {
#endif
struct trigger;
void xl_disarm_clear(void);
int xl_robot_disarmed(int object_index);
int xl_disarm_action_valid(const struct trigger *action);
int xl_disarm_action(const struct trigger *action);
#ifdef __cplusplus
}
#endif
#endif
