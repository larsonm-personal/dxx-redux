#ifndef DXX_XL_PREVIEW_H
#define DXX_XL_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xl_preview_vertex(int vertex, int32_t light, int32_t *r, int32_t *g, int32_t *b);
int xl_preview_start(void);
int xl_preview_load_level(const char *filename);
int xl_preview_wall_is_doorway(int segment_index, int side);
void xl_preview_before_swap(void);
uint64_t xl_preview_vertex_calls(void);
void xl_preview_begin_face(int segment, int side);
void xl_preview_end_face(void);
float xl_preview_alpha(float stock_alpha);
void xl_preview_dynamic_frame(void);
void xl_preview_muzzle_snapshot(int64_t current_time);
void xl_preview_player_thrust(int player, int32_t x, int32_t y, int32_t z);
void xl_preview_reset_player_thrust(void);
const char *xl_preview_level_path(const char *filename);
unsigned xl_preview_unresolved_emitters(void);
int xl_preview_pilot_active(void);
void xl_preview_complete_pilot(void);
int xl_preview_pilot_completed(void);
void xl_preview_trigger_result(int trigger, int shot, int result);
uint64_t xl_preview_trigger_mask(void);
#ifdef __cplusplus
}
#endif
#endif
