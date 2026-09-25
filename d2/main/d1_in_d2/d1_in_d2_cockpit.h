/* Original D1 cockpit operations using shared engine rendering */
#ifndef D1_IN_D2_COCKPIT_H
#define D1_IN_D2_COCKPIT_H
#include "fix.h"
#include "gr.h"
/* Inactive dispatch returns without drawing or mutating state */
int d1_in_d2_init_cockpit(void);
int d1_in_d2_draw_cockpit(void);
int d1_in_d2_render_gauges(int *added_score, fix *added_time);
int d1_in_d2_cockpit_window_canvas(int window, grs_canvas *canvas);
int d1_in_d2_draw_cockpit_window_overlay(void);
/* HUD bitmap resolution follows the installed assets, not the screen size */
int d1_in_d2_hud_hires(void);
void d1_in_d2_reset_cockpit(void);
void d1_in_d2_close_cockpit(void);
#endif
