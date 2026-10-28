#ifndef DXX_XL_PROJECTILE_H
#define DXX_XL_PROJECTILE_H
#include <stdint.h>
#define XL_PROJECTILE_HITS 20
#ifdef __cplusplus
extern "C" {
#endif
typedef struct xl_projectile_state {
	int32_t owner, signature, count;
	int32_t victims[XL_PROJECTILE_HITS];
} xl_projectile_state;
void xl_projectile_clear(void);
void xl_projectile_forget(int owner);
int xl_projectile_add_hit(int owner, int victim);
int xl_projectile_has_hit(int owner, int victim);
int xl_projectile_state_get(int owner, xl_projectile_state *state);
int xl_projectile_restore(const xl_projectile_state *states, int count, int apply);
void xl_projectile_impact(int owner, int other);
#ifdef __cplusplus
}
#endif
#endif
