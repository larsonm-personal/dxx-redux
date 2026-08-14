#include <assert.h>
#include <string.h>

#include "boss_health_shared.h"
#include "boss_hud.h"
#include "font_control_shared.h"
#include "hud_layout_shared.h"

static int visible_character_count(const char *text)
{
	int count = 0;

	while (*text) {
		const int control_length = font_draw_control_sequence_length(text);
		if (control_length)
			text += control_length;
		else {
			count++;
			text++;
		}
	}
	return count;
}

static void test_embedded_font_controls_are_not_visible(void)
{
	const char guidebot_text[] = {
		1, 29, 'G', 'u', 'i', 'd', 'e', '-', 'B', 'o', 't', ':', ' ',
		3, 'G', 'o', 'i', 'n', 'g', 4, 0
	};

	assert(font_draw_control_sequence_length(guidebot_text) == 2);
	assert(font_draw_control_sequence_length(guidebot_text + 13) == 1);
	assert(font_draw_control_sequence_length(guidebot_text + 19) == 1);
	assert(visible_character_count(guidebot_text) ==
		(int)strlen("Guide-Bot: Going"));
}

static void test_overlap(void)
{
	const hud_layout_rect a = { 10, 20, 30, 40 };
	const hud_layout_rect b = { 39, 59, 5, 5 };

	assert(hud_layout_rects_intersect(&a, &b));
	assert(hud_layout_rects_intersect(&b, &a));
}

static void test_touching_edges_do_not_overlap(void)
{
	const hud_layout_rect a = { 10, 20, 30, 40 };
	const hud_layout_rect right = { 40, 20, 5, 5 };
	const hud_layout_rect below = { 10, 60, 5, 5 };

	assert(!hud_layout_rects_intersect(&a, &right));
	assert(!hud_layout_rects_intersect(&a, &below));
}

static void test_containment_and_negative_coordinates(void)
{
	const hud_layout_rect outer = { -10, -10, 30, 30 };
	const hud_layout_rect inner = { 0, 0, 1, 1 };

	assert(hud_layout_rects_intersect(&outer, &inner));
}

static void test_empty_rectangles_do_not_overlap(void)
{
	const hud_layout_rect normal = { 0, 0, 10, 10 };
	const hud_layout_rect zero_width = { 5, 5, 0, 5 };
	const hud_layout_rect negative_height = { 5, 5, 5, -1 };

	assert(!hud_layout_rects_intersect(&normal, &zero_width));
	assert(!hud_layout_rects_intersect(&normal, &negative_height));
	assert(!hud_layout_rects_intersect(0, &normal));
}

static void test_padding_can_turn_a_near_miss_into_overlap(void)
{
	const hud_layout_rect message = { 10, 10, 10, 10 };
	const hud_layout_rect unpadded_item = { 20, 10, 5, 5 };
	const hud_layout_rect padded_item = { 19, 9, 7, 7 };

	assert(!hud_layout_rects_intersect(&message, &unpadded_item));
	assert(hud_layout_rects_intersect(&message, &padded_item));
}

static void test_boss_health_bar_widths(void)
{
	assert(boss_hud_green_width(100, 100, 98) == 98);
	assert(boss_hud_green_width(99, 100, 98) == 97);
	assert(boss_hud_green_width(50, 100, 98) == 49);
	assert(boss_hud_green_width(1, 100, 98) == 1);
	assert(boss_hud_green_width(0, 100, 98) == 0);
	assert(boss_hud_green_width(-1, 100, 98) == 0);
	assert(boss_hud_green_width(101, 100, 98) == 98);
	assert(boss_hud_green_width(1, 0, 98) == 0);
	assert(boss_hud_green_width(1, 100, 0) == 0);
}

static void test_boss_health_difficulty_scaling(void)
{
	const int32_t rookie_maximum = 239616000;
	const int32_t trainee_maximum = 95846400;

	assert(boss_health_rescale_value(rookie_maximum, rookie_maximum,
	                                 trainee_maximum) == trainee_maximum);
	assert(boss_health_rescale_value(rookie_maximum / 2, rookie_maximum,
	                                 trainee_maximum) == trainee_maximum / 2);
	assert(boss_health_rescale_value(trainee_maximum, trainee_maximum,
	                                 rookie_maximum) == rookie_maximum);
	assert(boss_health_rescale_value(1, rookie_maximum, trainee_maximum) == 1);
	assert(boss_health_rescale_value(0, rookie_maximum, trainee_maximum) == 0);
	assert(boss_health_rescale_value(-1, rookie_maximum, trainee_maximum) == -1);
}

static void test_d2_thief_and_guidebot_health(void)
{
	const int32_t strength = 80 * 65536;
	const int32_t level_maximum = 100 * 65536;

	assert(d2_thief_or_companion_health_maximum(strength, 3, 0, 0) ==
	       level_maximum);
	assert(d2_thief_or_companion_health_maximum(strength, -3, 4, 0) ==
	       level_maximum);
	assert(d2_thief_or_companion_health_maximum(strength, 3, 0, 1) ==
	       20000 * 65536);
	assert(d2_thief_or_companion_health_maximum(strength, 3, 1, 1) ==
	       level_maximum * 3);
	assert(d2_thief_or_companion_health_maximum(strength, 3, 2, 1) ==
	       level_maximum * 2);
	assert(d2_thief_or_companion_health_maximum(strength, 3, 4, 1) ==
	       level_maximum);
}

int main(void)
{
	test_embedded_font_controls_are_not_visible();
	test_overlap();
	test_touching_edges_do_not_overlap();
	test_containment_and_negative_coordinates();
	test_empty_rectangles_do_not_overlap();
	test_padding_can_turn_a_near_miss_into_overlap();
	test_boss_health_bar_widths();
	test_boss_health_difficulty_scaling();
	test_d2_thief_and_guidebot_health();
	return 0;
}
