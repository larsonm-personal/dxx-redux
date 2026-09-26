#include "android_virtual_gamepad.h"

#include <stdio.h>
#include "strutil.h"

void android_virtual_gamepad_init(int *axis_map, int *button_map,
                                  int *axis_button_map, char **axis_text,
                                  char **button_text)
{
	static const char *axis_names[] = { "LX", "LY", "RX", "RY", "LT", "RT", "BK", "SU" };
	static const char *btn_names[] = { "A", "B", "X", "Y", "L1", "R1", "Sel", "Sta", "L3", "R3" };
	static const char *dpad_names[] = { "DUp", "DDn", "DLt", "DRt" };
	char temp[64];
	int j;

	for (j = 0; j < ANDROID_VIRTUAL_GAMEPAD_BASE_AXES; j++) {
		sprintf(temp, "J1 %s", axis_names[j]);
		axis_text[j] = d_strdup(temp);
		axis_map[j] = j;
	}
	for (j = 0; j < ANDROID_VIRTUAL_GAMEPAD_BASE_BUTTONS; j++) {
		sprintf(temp, "J1 %s", btn_names[j]);
		button_text[j] = d_strdup(temp);
		button_map[j] = j;
	}
	/* Axis buttons only for physical axes 0-5; axes 6-7 are
	 * virtual (gyro/slide) and don't need directional buttons
	 * Set them to -1 so joy_axisbutton_handler() skips them */
	for (j = 0; j < 6; j++) {
		int button = ANDROID_VIRTUAL_GAMEPAD_BASE_BUTTONS + j * 2;
		axis_button_map[j] = button;
		sprintf(temp, "J1 -%s", axis_names[j]);
		button_text[button] = d_strdup(temp);
		sprintf(temp, "J1 +%s", axis_names[j]);
		button_text[button + 1] = d_strdup(temp);
	}
	for (j = 6; j < ANDROID_VIRTUAL_GAMEPAD_BASE_AXES; j++)
		axis_button_map[j] = -1;

	/* Virtual combiner axes for half-axis trigger bindings
	 * Kotlin computes combined values and sends via nativeJoystickAxis()
	 * axis_button_map must be -1 so joy_axisbutton_handler skips them;
	 * the memset-zero default (0) would cause spurious button-0/1 events */
	for (j = 0; j < 3; j++) {
		int axis = ANDROID_VIRTUAL_GAMEPAD_BASE_AXES + j;
		axis_map[axis] = axis;
		axis_button_map[axis] = -1;
		sprintf(temp, "J1 VC%d", j);
		axis_text[axis] = d_strdup(temp);
	}

	/* D-pad virtual buttons: DUp=22, DDown=23, DLeft=24, DRight=25
	 * Shared constant with MainActivity.kt DPAD_JOY_BUTTON_BASE
	 * Must set button_map[] so joy_button_handler() translates the
	 * raw SDL button index to the correct virtual button number */
	for (j = 0; j < 4; j++) {
		int button = 22 + j;
		sprintf(temp, "J1 %s", dpad_names[j]);
		button_map[button] = button;
		button_text[button] = d_strdup(temp);
	}
}
