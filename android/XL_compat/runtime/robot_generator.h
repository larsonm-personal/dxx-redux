#ifndef XL_ROBOT_GENERATOR_H
#define XL_ROBOT_GENERATOR_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct vms_vector;
void xl_robot_generator_load(void);
void xl_robot_generator_clear(void);
uint32_t xl_robot_generator_implicit_flags(void);
int xl_robot_generator_has_types(int center);
int xl_robot_generator_type(int center);
int32_t xl_robot_generator_shields(int type);
int32_t xl_robot_generator_distance(const struct vms_vector *a, const struct vms_vector *b);
int xl_robot_generator_validate(void);
int xl_robot_generator_contact(int object_index);
#ifdef __cplusplus
}
#endif
#endif
