#include "kconfig_android_shared.h"

#include <string.h>

static const unsigned char invert_indices[] = { 14, 16, 18, 20, 22, 24 };

static const struct kconfig_android_binding common_defaults[] = {
	{ 2, 21 }, { 3, 19 }, { 13, 3 }, { 15, 2 }, { 17, 0 }, { 23, 1 }, { 6, 24 }, { 7, 25 }, { 8, 22 }, { 9, 23 }, { 19, 7 }, { 21, 6 }, { 4, 2 }
};

static const struct kconfig_android_binding d1_defaults[] = {
	{ 27, 6 }, { 44, 4 }, { 45, 5 }
};

static const struct kconfig_android_binding d2_defaults[] = {
	{ 27, 3 }, { 28, 4 }, { 29, 5 }, { 50, 6 }
};

static const struct kconfig_android_layout layouts[] = {
	{ 50, invert_indices, sizeof(invert_indices), common_defaults,
	  sizeof(common_defaults) / sizeof(common_defaults[0]), d1_defaults,
	  sizeof(d1_defaults) / sizeof(d1_defaults[0]) },
	{ 56, invert_indices, sizeof(invert_indices), common_defaults,
	  sizeof(common_defaults) / sizeof(common_defaults[0]), d2_defaults,
	  sizeof(d2_defaults) / sizeof(d2_defaults[0]) }
};

const struct kconfig_android_layout *kconfig_android_get_layout(
    enum kconfig_android_game game)
{
	if (game != KCONFIG_ANDROID_D1 && game != KCONFIG_ANDROID_D2)
		return NULL;
	return &layouts[game];
}

void kconfig_android_fill_joy_settings(
    const struct kconfig_android_layout *layout, const int *indices,
    const int *values, int count, unsigned char *out, size_t out_size)
{
	size_t i;

	if (!layout || !out)
		return;
	memset(out, 0xff, out_size);
	for (i = 0; i < layout->invert_count; i++)
		if (layout->invert_indices[i] < out_size)
			out[layout->invert_indices[i]] = 0;
	if (!indices || !values || count < 0)
		return;
	for (i = 0; i < (size_t) count; i++)
		if (indices[i] >= 0 && (size_t) indices[i] < out_size)
			out[indices[i]] = (unsigned char) (values[i] & 0xff);
}

void kconfig_android_fill_kb_settings(const unsigned char *defaults,
                                      const int *indices, const int *values, int count, unsigned char *out,
                                      size_t out_size)
{
	size_t i;

	if (!defaults || !out)
		return;
	memcpy(out, defaults, out_size);
	if (!indices || !values || count < 0)
		return;
	for (i = 0; i < (size_t) count; i++)
		if (indices[i] >= 0 && (size_t) indices[i] < out_size)
			out[indices[i]] = (unsigned char) (values[i] & 0xff);
}

static void apply_bindings(const struct kconfig_android_binding *bindings,
                           size_t count, unsigned char *joy, size_t joy_size)
{
	size_t i;

	for (i = 0; i < count; i++)
		if (bindings[i].index < joy_size)
			joy[bindings[i].index] = bindings[i].value;
}

void kconfig_android_apply_default_overrides(
    const struct kconfig_android_layout *layout, unsigned char *joy,
    size_t joy_size)
{
	if (!layout || !joy)
		return;
	apply_bindings(layout->common_defaults, layout->common_default_count,
	               joy, joy_size);
	apply_bindings(layout->game_defaults, layout->game_default_count,
	               joy, joy_size);
}
