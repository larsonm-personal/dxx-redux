#include <assert.h>
#include <string.h>

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

int main(void)
{
	test_embedded_font_controls_are_not_visible();
	test_overlap();
	test_touching_edges_do_not_overlap();
	test_containment_and_negative_coordinates();
	test_empty_rectangles_do_not_overlap();
	test_padding_can_turn_a_near_miss_into_overlap();
	return 0;
}
