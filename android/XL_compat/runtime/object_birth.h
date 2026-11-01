#ifndef DXX_XL_OBJECT_BIRTH_H
#define DXX_XL_OBJECT_BIRTH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xl_object_birth_clear(void);
void xl_object_birth_created(int owner);
int64_t xl_object_birth_time(int owner);
#ifdef __cplusplus
}
#endif
#endif
