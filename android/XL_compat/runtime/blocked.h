#ifndef DXX_XL_BLOCKED_H
#define DXX_XL_BLOCKED_H
#ifdef __cplusplus
extern "C" {
#endif
int xl_blocked_segment(int segment);
int xl_blocked_map_segment(int segment);
int xl_blocked_entry(int object_index, int segment, int side);
int xl_blocked_map_color(int segment, int fallback);
unsigned xl_blocked_map_calls(void);
unsigned xl_blocked_projectile_entries(void);
void xl_blocked_note_move(int object_index, int previous_segment);
void xl_blocked_reset(void);
#ifdef __cplusplus
}
#endif
#endif
