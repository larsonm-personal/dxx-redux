#ifndef DXX_XL_FLICKER_H
#define DXX_XL_FLICKER_H
#include <stdint.h>
struct xl_flicker_state {
	int segment, side, delay, timer;
	uint32_t mask;
};
#ifdef __cplusplus
namespace xl_compat
{
struct Inventory;
}
void xl_flicker_bind(const xl_compat::Inventory *source);
extern "C" {
#endif
void xl_flicker_clear(void);
int xl_flicker_count(void);
int xl_flicker_get(int index, struct xl_flicker_state *out);
int xl_flicker_has_source(int segment, int side);
int xl_flicker_frame(void);
int xl_flicker_enable(int segment, int side, int enabled);
int xl_flicker_step(struct xl_flicker_state *state, int elapsed, int visible, int emitting);
int xl_flicker_restore(const struct xl_flicker_state *states, int count, int apply);
#ifdef __cplusplus
}
#endif
#endif
