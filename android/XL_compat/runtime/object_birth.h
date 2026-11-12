#ifndef DXX_XL_OBJECT_BIRTH_H
#define DXX_XL_OBJECT_BIRTH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xl_object_birth_clear(void);
void xl_object_birth_created(int owner);
int64_t xl_object_birth_time(int owner);
uint64_t xl_object_birth_order(int owner);
#ifdef __cplusplus
}
#include <nlohmann/json_fwd.hpp>
nlohmann::json xl_object_birth_snapshot();
bool xl_object_birth_restore(const nlohmann::json &saved, bool apply);
#endif
#endif
