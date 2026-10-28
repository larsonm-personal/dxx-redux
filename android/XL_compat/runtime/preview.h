#ifndef DXX_XL_PREVIEW_H
#define DXX_XL_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int xl_preview_water_segment(int segment);
int xl_preview_no_damage_segment(int segment);
unsigned xl_preview_rebuilt_ambient_vertices(void);
int xl_preview_preserve_fusion_queue(void);
#ifdef __ANDROID__
/* Automation-only opt-in; refuse to change the profile during gameplay */
int xl_preview_set_water_test_profile(int enabled);
int xl_preview_set_classic_light_test_profile(int enabled);
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
/* Experimental classic companion lighting; prepare after level asset loading */
int xl_classic_lighting_prepare(const char *filename);
int xl_classic_lighting_requested(void);
void xl_classic_lighting_release(void);
int xl_classic_lighting_color(int vertex, float rgb[3]);
unsigned xl_classic_lighting_vertex_count(void);
/* No-mesh visual profile belongs to prepared classic lighting, not mission identity */
int xl_classic_legacy_fans_active(void);
void xl_preview_clear_mission(void);
int xl_level_supported(const char *filename, int multiplayer);
int xl_level_segment_group(int segment);
int xl_source_level_version(void);
int xl_source_game_version(void);
const char *xl_level_rejection(void);
int xl_test_stop_after_level(void);
int xl_preview_wall_is_doorway(int segment_index, int side);
void xl_preview_before_swap(void);
/* Explicit still-view diagnostic only; does not change collision geometry */
int xl_preview_reference_fans(void);
/* Raw displacement and truncating dot test; retains engine compiled normals */
int xl_preview_split_face_visible(int segment, int side, int32_t x, int32_t y, int32_t z);
uint64_t xl_preview_vertex_calls(void);
void xl_preview_begin_face(int segment, int side);
void xl_preview_end_face(void);
int xl_preview_water_portal(int segment, int side, int32_t points[4][3], float colors[4][4]);
void xl_preview_draw_water_portal(int segment, int side);
void ogl_draw_xl_water_portal(const int32_t points[4][3], const float colors[4][4]);
void xl_preview_record_water_draw(int depth_enabled);
uint64_t xl_preview_water_draws(void);
uint64_t xl_preview_water_depth_failures(void);
float xl_preview_alpha(float stock_alpha);
int xl_preview_begin_cloak_face(int segment, int side);
void xl_preview_record_cloak_draw(float alpha);
uint64_t xl_preview_cloak_draws(void);
int xl_preview_cloak_draw_wall(void);
int xl_preview_cloak_draw_value(void);
float xl_preview_cloak_draw_alpha(void);
void xl_preview_dynamic_frame(void);
void xl_preview_muzzle_snapshot(int64_t current_time);
void xl_preview_player_thrust(int player, int32_t x, int32_t y, int32_t z);
void xl_preview_reset_player_thrust(void);
const char *xl_preview_level_path(const char *filename);
/* Transient permissions for verified private XL level adapters */
int xl_loading_wide_walls(void);
int xl_loading_extended_triggers(void);
unsigned xl_preview_unresolved_emitters(void);
int xl_preview_pilot_active(void);
int xl_runtime_active(void);
int xl_level_extensions_active(void);
int xl_wall_teleport_action(int trigger_index, int object_index, int shot);
void xl_wall_teleport_crossings(int object_index, const int *segments, int count);
int xl_wall_teleport_actor(int object_index);
int xl_wall_trigger(int trigger_index, int object_index, int shot);
void xl_wall_trigger_reset(void);
void xl_wall_trigger_exit(void);
void xl_wall_trigger_frame(void);
void xl_object_trigger_event(int object_index, int damage);
int xl_wall_trigger_stock_active(void);
struct trigger;
int xl_trigger_cue_enabled(const struct trigger *action, int sound);
int xl_trigger_illusion_action(const struct trigger *action);
int xl_authored_close_door(int segment, int side);
int xl_player_resource_trigger(const struct trigger *action);
/* Debug-only process-cumulative cue decisions; not evidence of mixer playback */
uint64_t xl_trigger_cue_queries(int sound, int enabled);
int xl_segment_sound_count(int segment, int side, int sound, int playing_only);
void xl_draw_menu_indicator(void);
uint64_t xl_menu_indicator_draws(void);
void xl_preview_complete_pilot(void);
int xl_preview_pilot_completed(void);
void xl_preview_trigger_result(int trigger, int shot, int result);
uint64_t xl_preview_trigger_mask(void);
// Diagnostic since the current level load, not persisted scheduler state
int xl_preview_trigger_activated(int index);
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
