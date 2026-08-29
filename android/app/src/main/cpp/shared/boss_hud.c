#include <string.h>

#include "boss_health_shared.h"
#include "boss_hud.h"
#include "game.h"
#include "gamefont.h"
#include "gr.h"
#include "inferno.h"
#include "object.h"
#include "playsave.h"
#include "robot.h"

static int Boss_hud_objnum = -1;
static int Boss_hud_signature = -1;
static fix Boss_hud_maximum_shields;
static int Boss_hud_row_x;
static int Boss_hud_row_y = -1;
static int Boss_hud_label_width;
static int Boss_hud_bar_x;
static int Boss_hud_bar_y;
static int Boss_hud_bar_width;
static int Boss_hud_bar_height;
static boss_hud_debug_state Boss_hud_debug;

static fix boss_hud_full_shields(const object *boss)
{
	fix maximum = boss_health_maximum_for_difficulty(
	    Robot_info[boss->id].strength, Difficulty_level);
	if (boss->shields > maximum)
		maximum = boss->shields;
	return maximum;
}

static object *boss_hud_get_active(void)
{
	object *boss;

	if (Boss_hud_objnum < 0 || Boss_hud_objnum > Highest_object_index)
		return NULL;
	boss = &Objects[Boss_hud_objnum];
	if (boss->signature != Boss_hud_signature || boss->type != OBJ_ROBOT ||
	    !Robot_info[boss->id].boss_flag) {
		boss_hud_reset();
		return NULL;
	}
	return boss;
}

void boss_hud_refresh_maximum(void)
{
	object *boss = boss_hud_get_active();

	if (boss)
		Boss_hud_maximum_shields = boss_hud_full_shields(boss);
}

void boss_hud_note_shot(int objnum)
{
	object *boss;

	if (objnum < 0 || objnum > Highest_object_index)
		return;
	boss = &Objects[objnum];
	if (boss->type != OBJ_ROBOT || !Robot_info[boss->id].boss_flag)
		return;
	if (boss_hud_get_active() && Boss_hud_objnum != objnum)
		return;
	Boss_hud_objnum = objnum;
	Boss_hud_signature = boss->signature;
	Boss_hud_maximum_shields = boss_hud_full_shields(boss);
}

int boss_hud_note_weapon_collision(const object *target, const object *weapon)
{
	int boss_objnum = -1;

	if (!target || !weapon || weapon->type != OBJ_WEAPON)
		return -1;
	if (target->type == OBJ_ROBOT && Robot_info[target->id].boss_flag &&
	    weapon->ctype.laser_info.parent_type == OBJ_PLAYER)
		boss_objnum = target - Objects;
	else if (target->type == OBJ_PLAYER &&
	         weapon->ctype.laser_info.parent_type == OBJ_ROBOT &&
	         weapon->ctype.laser_info.parent_num >= 0 &&
	         weapon->ctype.laser_info.parent_num <= Highest_object_index) {
		const object *boss = &Objects[weapon->ctype.laser_info.parent_num];

		if (boss->signature == weapon->ctype.laser_info.parent_signature &&
		    boss->type == OBJ_ROBOT && Robot_info[boss->id].boss_flag)
			boss_objnum = weapon->ctype.laser_info.parent_num;
	}
	if (boss_objnum >= 0)
		boss_hud_note_shot(boss_objnum);
	return boss_objnum;
}

void boss_hud_reset(void)
{
	Boss_hud_objnum = -1;
	Boss_hud_signature = -1;
	Boss_hud_maximum_shields = 0;
	Boss_hud_row_y = -1;
}

int boss_hud_is_visible(void)
{
	return PlayerCfg.ShowBossHealthBar && boss_hud_get_active() != NULL;
}

int boss_hud_message_capacity(int normal_capacity)
{
	return boss_hud_is_visible() && normal_capacity > 0 ? normal_capacity - 1 : normal_capacity;
}

int boss_hud_prepare_row(int y, int queued_message_capacity,
                         int queued_message_draw_count, hud_layout_rect *rect)
{
	object *boss = boss_hud_get_active();
	const int visible = PlayerCfg.ShowBossHealthBar && boss != NULL;

	memset(&Boss_hud_debug, 0, sizeof(Boss_hud_debug));
	Boss_hud_row_y = -1;
	Boss_hud_debug.enabled = PlayerCfg.ShowBossHealthBar != 0;
	Boss_hud_debug.active = boss != NULL;
	Boss_hud_debug.visible_slot = -1;
	Boss_hud_debug.objnum = boss ? Boss_hud_objnum : -1;
	Boss_hud_debug.signature = boss ? Boss_hud_signature : -1;
	Boss_hud_debug.shields = boss ? boss->shields : 0;
	Boss_hud_debug.maximum_shields = boss ? Boss_hud_maximum_shields : 0;
	Boss_hud_debug.queued_message_capacity = queued_message_capacity;
	Boss_hud_debug.queued_message_draw_count = queued_message_draw_count;
	if (!visible || !rect)
		return 0;

	{
		const int pad_x = FSPACX(1);
		const int pad_y = FSPACY(1);
		int label_height;

		gr_get_string_drawn_size(BOSS_HUD_LABEL, &Boss_hud_label_width, &label_height);
		Boss_hud_bar_width = max(1, (int) (100 * FNTScaleX) - 2);
		Boss_hud_bar_height = max(1, (int) (4 * FNTScaleY));
		Boss_hud_row_x = (grd_curcanv->cv_bitmap.bm_w -
		                  Boss_hud_label_width - Boss_hud_bar_width) /
		                 2;
		Boss_hud_row_y = y;
		Boss_hud_bar_x = Boss_hud_row_x + Boss_hud_label_width;
		Boss_hud_bar_y = y + max(0, (label_height - Boss_hud_bar_height) / 2);
		rect->x = Boss_hud_row_x - pad_x;
		rect->y = y - pad_y;
		rect->w = Boss_hud_label_width + Boss_hud_bar_width + 2 * pad_x;
		rect->h = max(label_height, Boss_hud_bar_height) + 2 * pad_y;
	}

	Boss_hud_debug.drawn = 1;
	Boss_hud_debug.visible_slot = 0;
	Boss_hud_debug.label_width = Boss_hud_label_width;
	Boss_hud_debug.bar_width = Boss_hud_bar_width;
	Boss_hud_debug.bar_height = Boss_hud_bar_height;
	Boss_hud_debug.green_width = boss_hud_green_width(
	    boss->shields, Boss_hud_maximum_shields, Boss_hud_bar_width);
	Boss_hud_debug.red_width = Boss_hud_bar_width - Boss_hud_debug.green_width;
	Boss_hud_debug.bar_x = Boss_hud_bar_x;
	Boss_hud_debug.bar_y = Boss_hud_bar_y;
	Boss_hud_debug.row_x = rect->x;
	Boss_hud_debug.row_y = rect->y;
	Boss_hud_debug.row_w = rect->w;
	Boss_hud_debug.row_h = rect->h;
	return 1;
}

int boss_hud_row_is_prepared(void)
{
	return Boss_hud_row_y >= 0;
}

void boss_hud_render(int hud_color)
{
	object *boss = boss_hud_get_active();
	int green_width;

	if (!boss || Boss_hud_row_y < 0)
		return;
	green_width = boss_hud_green_width(
	    boss->shields, Boss_hud_maximum_shields, Boss_hud_bar_width);
	gr_set_fontcolor(hud_color, -1);
	gr_string(Boss_hud_row_x, Boss_hud_row_y, BOSS_HUD_LABEL);
	if (green_width > 0) {
		gr_setcolor(BM_XRGB(0, 28, 0));
		gr_rect(Boss_hud_bar_x, Boss_hud_bar_y,
		        Boss_hud_bar_x + green_width - 1,
		        Boss_hud_bar_y + Boss_hud_bar_height - 1);
	}
	if (green_width < Boss_hud_bar_width) {
		gr_setcolor(BM_XRGB(28, 0, 0));
		gr_rect(Boss_hud_bar_x + green_width, Boss_hud_bar_y,
		        Boss_hud_bar_x + Boss_hud_bar_width - 1,
		        Boss_hud_bar_y + Boss_hud_bar_height - 1);
	}
	Boss_hud_debug.shields = boss->shields;
	Boss_hud_debug.green_width = green_width;
	Boss_hud_debug.red_width = Boss_hud_bar_width - green_width;
}

const boss_hud_debug_state *boss_hud_get_debug_state(void)
{
	return &Boss_hud_debug;
}
