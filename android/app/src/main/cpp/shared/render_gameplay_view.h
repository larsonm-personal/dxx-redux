/* Separate visual-camera and ship-forward gameplay candidate collection */
#ifndef RENDER_GAMEPLAY_VIEW_H
#define RENDER_GAMEPLAY_VIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Uses the current canvas and viewer, including rear view and stereo offset
 * Writes at most MAX_RENDERED_OBJECTS slots, with the original overflow rule
 * Returns -1 during endlevel or without a valid view, leaving output untouched
 * Updates renderer scratch state, but does not draw or advance simulation */
int render_collect_view_objects(int32_t eye_offset, short *objects);
/* Fixed ship-forward DOS cockpit projection, see gameplay_view_policy.h
 * Independent of camera, screen, cockpit, stereo and visual FoV
 * Leaves visual camera lists, discovery and projection globals unchanged
 * Rebuilds renderer rotation/portal/object scratch: call only at simulation/input
 * boundaries, never from an in-progress render. The next visual consumer must
 * start its normal frame, invalidating rotation caches with render_start_frame
 * Uses original portal/object ordering and bounded overflow semantics */
int render_collect_gameplay_objects(short *objects);
/* Match g3 matrix scaling before rotation, including fixed-point rounding */
struct vms_matrix;
void render_gameplay_view_orientation(struct vms_matrix *scaled, const struct vms_matrix *orientation);
void render_update_gameplay_view_objects(void);
void render_prepare_gameplay_view_objects(void);
/* Explicit teleports/world replacement, including automation pose edits, must
 * invalidate before the next input. Ordinary movement must not invalidate: it
 * deliberately consumes the preceding simulation step's candidate snapshot */
void render_reset_gameplay_view_objects(void);
int render_get_gameplay_view_objects(const short **objects);
extern int Gameplay_num_rendered_objects;
extern short Gameplay_rendered_object_list[];

/* Publish the current main-view candidates when the draw is omitted
 * Returns zero if the main viewport has not been initialized */
int render_update_main_view_objects(void);
/* Apply ordinary main-camera map discovery when drawing is omitted
 * Uses Screen_3d_window, current Viewer/rear view, native projection and clipping
 * Leaves object candidate lists and RNG untouched; updates renderer scratch
 * Returns 1 when handled (including map-active/robot-camera no-ops), or 0 for
 * endlevel/invalid view. Caller owns guided-big-window/subview selection and
 * must not treat this as endlevel or auxiliary-camera rendering equivalence */
int render_update_main_view_automap(void);
/* Main view plus native default D2 missile window, without drawing or AI wake */
int render_update_game_view_automap(void);
/* Null when this bounded discovery path supports the actively selected cameras */
const char *render_game_view_automap_unsupported_reason(void);
#ifdef ANDROID
/* Baseline FOV bookkeeping without starting a GPU frame or preparing lighting */
void android_render_collect_fov_visibility(int32_t eye_offset, int window_num);
struct object;
void android_render_record_visible_object(struct object *obj);
#ifdef INTROSPECT_ON
void android_render_visibility_verify_set(int enabled);
int android_render_visibility_verify_enabled(void);
void android_render_visibility_verify_begin(void);
void android_render_visibility_verify_reference(int window_num);
void android_render_visibility_verify_compare(int window_num);
unsigned int android_render_visibility_verify_checks(void);
unsigned int android_render_visibility_verify_failures(void);
#endif
#endif
#ifdef __cplusplus
}
#endif
#endif
