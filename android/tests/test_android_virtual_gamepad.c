#include "android_virtual_gamepad.h"
#include "strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "virtual gamepad contract failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
} } while (0)

/* The engine allocator is independent of the fixed virtual-ID contract */
char *d_strdup(char *text)
{
	size_t size = strlen(text) + 1;
	char *copy = malloc(size);
	if (!copy)
		abort();
	memcpy(copy, text, size);
	return copy;
}

int main(void)
{
	/* These IDs are persisted in player bindings and sent by Kotlin input */
	static const char *expected_axes[] = {
		"J1 LX", "J1 LY", "J1 RX", "J1 RY", "J1 LT", "J1 RT",
		"J1 BK", "J1 SU", "J1 VC0", "J1 VC1", "J1 VC2"
	};
	static const char *expected_buttons[] = {
		"J1 A", "J1 B", "J1 X", "J1 Y", "J1 L1", "J1 R1", "J1 Sel", "J1 Sta", "J1 L3", "J1 R3",
		"J1 -LX", "J1 +LX", "J1 -LY", "J1 +LY", "J1 -RX", "J1 +RX",
		"J1 -RY", "J1 +RY", "J1 -LT", "J1 +LT", "J1 -RT", "J1 +RT",
		"J1 DUp", "J1 DDn", "J1 DLt", "J1 DRt"
	};
	int axis_map[16], axis_button_map[16], button_map[32];
	char *axis_text[16] = { 0 }, *button_text[32] = { 0 };
	int i;
	for (i = 0; i < 16; i++)
		axis_map[i] = axis_button_map[i] = -99;
	for (i = 0; i < 32; i++)
		button_map[i] = -99;

	CHECK(ANDROID_VIRTUAL_GAMEPAD_BASE_AXES == 8);
	CHECK(ANDROID_VIRTUAL_GAMEPAD_BASE_BUTTONS == 10);
	CHECK(ANDROID_VIRTUAL_GAMEPAD_AXES == 11);
	CHECK(ANDROID_VIRTUAL_GAMEPAD_BUTTONS == 26);
	android_virtual_gamepad_init(axis_map, button_map, axis_button_map,
	                            axis_text, button_text);
	for (i = 0; i < 16; i++) {
		if (i < 11) {
			CHECK(axis_map[i] == i);
			CHECK(axis_text[i] && !strcmp(axis_text[i], expected_axes[i]));
			/* Gyro/slide/combiners must never generate axis-button 0/1 */
			CHECK(axis_button_map[i] == (i < 6 ? 10 + 2 * i : -1));
			free(axis_text[i]);
		} else {
			CHECK(axis_map[i] == -99 && axis_button_map[i] == -99);
			CHECK(axis_text[i] == NULL);
		}
	}
	for (i = 0; i < 32; i++) {
		if (i < 26) {
			CHECK(button_text[i] && !strcmp(button_text[i], expected_buttons[i]));
			free(button_text[i]);
		} else {
			CHECK(button_text[i] == NULL);
		}
		/* Axis buttons are synthesized directly, leaving their SDL map unused */
		CHECK(button_map[i] == (i < 10 || (i >= 22 && i < 26) ? i : -99));
	}
	return 0;
}
