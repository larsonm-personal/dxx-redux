#ifndef ANDROID_VIRTUAL_GAMEPAD_H
#define ANDROID_VIRTUAL_GAMEPAD_H

/* Virtual IDs shared with TouchBindings.kt and MainActivity.kt */
#define ANDROID_VIRTUAL_GAMEPAD_BASE_AXES    8
#define ANDROID_VIRTUAL_GAMEPAD_BASE_BUTTONS 10
#define ANDROID_VIRTUAL_GAMEPAD_AXES         11
#define ANDROID_VIRTUAL_GAMEPAD_BUTTONS      26

/* Arrays need AXES/BUTTONS entries; names use the engine's d_strdup allocator
 * Only registered slots are written, preserving unused SDL map entries */
void android_virtual_gamepad_init(int *axis_map, int *button_map,
                                  int *axis_button_map, char **axis_text,
                                  char **button_text);

#endif
