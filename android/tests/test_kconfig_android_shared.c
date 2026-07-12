#include "kconfig_android_shared.h"

#include <stdio.h>
#include <string.h>

static int check_game(enum kconfig_android_game game, size_t size,
	const struct kconfig_android_binding *expected, size_t expected_count)
{
	const struct kconfig_android_layout *layout =
		kconfig_android_get_layout(game);
	unsigned char joy[64];
	size_t i;

	memset(joy, 0xee, sizeof(joy));
	kconfig_android_apply_default_overrides(layout, joy, size);
	for (i = 0; i < expected_count; i++)
		if (joy[expected[i].index] != expected[i].value)
			return 0;
	return 1;
}

int main(void)
{
	static const struct kconfig_android_binding d1_expected[] = {
		{2, 21}, {3, 19}, {4, 2}, {6, 24}, {7, 25}, {8, 22}, {9, 23},
		{13, 3}, {15, 2}, {17, 0}, {19, 7}, {21, 6}, {23, 1},
		{27, 6}, {44, 4}, {45, 5}
	};
	static const struct kconfig_android_binding d2_expected[] = {
		{2, 21}, {3, 19}, {4, 2}, {6, 24}, {7, 25}, {8, 22}, {9, 23},
		{13, 3}, {15, 2}, {17, 0}, {19, 7}, {21, 6}, {23, 1},
		{27, 3}, {28, 4}, {29, 5}, {50, 6}
	};
	const int indices[] = {0, 14, 63, -1};
	const int values[] = {7, 1, 9, 4};
	unsigned char output[56];
	unsigned char defaults[56];
	size_t i;

	if (!check_game(KCONFIG_ANDROID_D1, 50, d1_expected,
	                sizeof(d1_expected) / sizeof(d1_expected[0])) ||
	    !check_game(KCONFIG_ANDROID_D2, 56, d2_expected,
	                sizeof(d2_expected) / sizeof(d2_expected[0])))
		return 1;
	if (kconfig_android_get_layout(KCONFIG_ANDROID_D1)->settings_size != 50 ||
	    kconfig_android_get_layout(KCONFIG_ANDROID_D2)->settings_size != 56)
		return 1;
	kconfig_android_fill_joy_settings(
		kconfig_android_get_layout(KCONFIG_ANDROID_D2), indices, values, 4,
		output, sizeof(output));
	for (i = 0; i < sizeof(output); i++) {
		unsigned char expected = 0xff;
		if (i == 14 || i == 16 || i == 18 || i == 20 || i == 22 || i == 24)
			expected = 0;
		if (i == 0)
			expected = 7;
		if (i == 14)
			expected = 1;
		if (output[i] != expected) {
			fprintf(stderr, "joystick slot %u mismatch\n", (unsigned)i);
			return 1;
		}
	}
	memset(defaults, 0x55, sizeof(defaults));
	kconfig_android_fill_kb_settings(defaults, indices, values, 4, output,
	                                sizeof(output));
	return output[0] == 7 && output[14] == 1 && output[55] == 0x55 ? 0 : 1;
}
