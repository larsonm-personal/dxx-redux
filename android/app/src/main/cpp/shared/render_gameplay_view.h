/* CPU collection of the objects offered by the engine's current view */
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
/* Publish the current main-view candidates when the draw is omitted
 * Returns zero if the main viewport has not been initialized */
int render_update_main_view_objects(void);
#ifdef __cplusplus
}
#endif
#endif
