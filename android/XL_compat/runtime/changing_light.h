#ifndef DXX_XL_CHANGING_LIGHT_H
#define DXX_XL_CHANGING_LIGHT_H

struct xl_light_index {
	int segment, side, count, first;
};
struct xl_light_delta {
	int segment, side, light[4];
};
struct xl_light_segment_state {
	int light[24], ambient;
};
#ifdef __cplusplus
namespace xl_compat
{
struct Inventory;
}
void xl_changing_light_bind(const xl_compat::Inventory *source);
extern "C" {
#endif
void xl_changing_light_clear(void);
int xl_changing_light(int segment, int side, int direction);
int xl_light_at_distance(int distance, int brightness);
int xl_light_index_count(void);
int xl_light_delta_count(void);
int xl_light_index_get(int index, struct xl_light_index *out);
int xl_light_delta_get(int index, struct xl_light_delta *out);
int xl_light_segment_count(void);
int xl_light_segment_get(int segment, struct xl_light_segment_state *out);
int xl_light_segment_restore(const struct xl_light_segment_state *states, int count, int apply);
#ifdef __cplusplus
}
#endif
#endif
