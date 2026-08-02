#ifndef BOSS_HUD_H
#define BOSS_HUD_H

#include <stdint.h>

#include "hud_layout_shared.h"

#define BOSS_HUD_LABEL "boss "

typedef struct boss_hud_debug_state {
	int enabled;
	int active;
	int drawn;
	int visible_slot;
	int objnum;
	int signature;
	int shields;
	int maximum_shields;
	int label_width;
	int bar_width;
	int bar_height;
	int green_width;
	int red_width;
	int bar_x;
	int bar_y;
	int row_x;
	int row_y;
	int row_w;
	int row_h;
	int queued_message_capacity;
	int queued_message_draw_count;
} boss_hud_debug_state;

static inline int boss_hud_green_width(int shields, int maximum_shields, int bar_width)
{
	int width;

	if (bar_width <= 0 || maximum_shields <= 0 || shields <= 0)
		return 0;
	if (shields >= maximum_shields)
		return bar_width;
	width = (int) (((int64_t) shields * bar_width) / maximum_shields);
	return width > 0 ? width : 1;
}

void boss_hud_note_active(int objnum);
void boss_hud_reset(void);
int boss_hud_is_visible(void);
int boss_hud_message_capacity(int normal_capacity);
int boss_hud_prepare_row(int y, int queued_message_capacity,
                         int queued_message_draw_count, hud_layout_rect *rect);
int boss_hud_row_is_prepared(void);
void boss_hud_render(int hud_color);
const boss_hud_debug_state *boss_hud_get_debug_state(void);

#endif
