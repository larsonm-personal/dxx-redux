#ifndef KCONFIG_ANDROID_SHARED_H
#define KCONFIG_ANDROID_SHARED_H

#include <stddef.h>

enum kconfig_android_game {
	KCONFIG_ANDROID_D1,
	KCONFIG_ANDROID_D2
};

struct kconfig_android_binding {
	unsigned char index;
	unsigned char value;
};

struct kconfig_android_layout {
	size_t settings_size;
	const unsigned char *invert_indices;
	size_t invert_count;
	const struct kconfig_android_binding *common_defaults;
	size_t common_default_count;
	const struct kconfig_android_binding *game_defaults;
	size_t game_default_count;
};

const struct kconfig_android_layout *kconfig_android_get_layout(
    enum kconfig_android_game game);
void kconfig_android_fill_joy_settings(
    const struct kconfig_android_layout *layout, const int *indices,
    const int *values, int count, unsigned char *out, size_t out_size);
void kconfig_android_fill_kb_settings(const unsigned char *defaults,
                                      const int *indices, const int *values, int count, unsigned char *out,
                                      size_t out_size);
void kconfig_android_apply_default_overrides(
    const struct kconfig_android_layout *layout, unsigned char *joy,
    size_t joy_size);

#endif
