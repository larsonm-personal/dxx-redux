/* android port: coop indicator lines -- path lines to guidebot/nearest player
 * Shared between D1 and D2 builds */
#ifndef COOP_INDICATOR_LINES_H
#define COOP_INDICATOR_LINES_H

#ifdef __ANDROID__

/* Call from render_frame() between render_mine() and g3_end_frame().
 * Handles both path update (throttled) and drawing. */
void coop_indicator_lines_render(void);

/* Apply launcher-local visibility toggles layered on top of server state. */
void coop_indicator_lines_set_options(int show_nearest_player, int show_guidebot);

/* Trigger per-frame diagnostic logging after a coop restore */
void coop_indicator_diag_trigger(void);

#endif /* __ANDROID__ */
#endif /* COOP_INDICATOR_LINES_H */
