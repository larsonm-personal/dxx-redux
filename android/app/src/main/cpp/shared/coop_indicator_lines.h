/* android port: coop indicator lines -- path lines to guidebot/nearest player
 * Shared between D1 and D2 builds */
#ifndef COOP_INDICATOR_LINES_H
#define COOP_INDICATOR_LINES_H

#ifdef __ANDROID__

/* Call from render_frame() between render_mine() and g3_end_frame().
 * Handles both path update (throttled) and drawing. */
void coop_indicator_lines_render(void);

#endif /* __ANDROID__ */
#endif /* COOP_INDICATOR_LINES_H */
