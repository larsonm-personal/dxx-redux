#ifndef DXX_XL_PREVIEW_H
#define DXX_XL_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int xl_preview_water_segment(int segment);
int xl_preview_no_damage_segment(int segment);
int xl_preview_preserve_fusion_queue(void);
#ifdef __ANDROID__
/* Automation-only opt-in; refuse to change the profile during gameplay */
int xl_preview_set_water_test_profile(int enabled);
#endif
int32_t xl_preview_motion_time(int segment, int32_t time);
int xl_preview_water_frame(int object_index);
int xl_preview_water_contact(int object_index, int segment);
int xl_preview_water_sound(int object_index, int side, int contact_sound);
int xl_preview_fluid_loop(int object_index, int sound, int playing);
int xl_linked_sound_count(int object_index, int sound, int playing_only);
void xl_preview_vertex(int vertex, int32_t light, int32_t *r, int32_t *g, int32_t *b);
int xl_preview_start(void);
int xl_preview_load_level(const char *filename);
int xl_level_supported(const char *filename, int multiplayer);
const char *xl_level_rejection(void);
int xl_test_stop_after_level(void);
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
int xl_runtime_active(void);
void xl_draw_menu_indicator(void);
uint64_t xl_menu_indicator_draws(void);
void xl_preview_complete_pilot(void);
int xl_preview_pilot_completed(void);
void xl_preview_trigger_result(int trigger, int shot, int result);
uint64_t xl_preview_trigger_mask(void);
typedef struct xl_texture_emitter {
	int texture;
	float rgb[3];
	float intensity;
} xl_texture_emitter;
// Live emitter eligibility/color; visibility and drawing are separate
int xl_preview_texture_emitter(int segment, int side, xl_texture_emitter *emitter);
int xl_preview_corona_geometry(int segment, int side, const int32_t eye[3], int eye_segment,
                               int32_t center[3], int32_t *radius, float *facing);
void xl_preview_render_emitters(const short *segments, int count, const int32_t eye[3], int eye_segment);
uint64_t xl_preview_corona_draws(void);
void xl_preview_record_corona_depth(int enabled, int classic_requested);
uint64_t xl_preview_corona_depth_failures(void);
uint64_t xl_preview_corona_classic_requested_draws(void);
int ogl_draw_xl_corona(const int32_t center[3], int32_t radius, const float rgb[3], float intensity);
void ogl_record_xl_corona_depth(void);
#ifdef __cplusplus
}
#endif
#endif
