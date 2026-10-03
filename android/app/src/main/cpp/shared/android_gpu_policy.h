#ifndef DXX_ANDROID_GPU_POLICY_H
#define DXX_ANDROID_GPU_POLICY_H
#include <string.h>

static inline int android_gpu_has_extension(const char *extensions, const char *name)
{
	if (!extensions || !name || !*name || strchr(name, ' ')) return 0;
	const size_t length = strlen(name);
	const char *match = extensions;
	while ((match = strstr(match, name)) != NULL) {
		if ((match == extensions || match[-1] == ' ') &&
		    (match[length] == 0 || match[length] == ' ')) return 1;
		match += length;
	}
	return 0;
}

/* Sized OpenGL ES internal-format values, kept independent of GL for host tests */
static inline unsigned int android_gpu_color_format(int r, int g, int b, int a)
{
	if (r == 10 && g == 10 && b == 10 && a == 2) return 0x8059; /* GL_RGB10_A2 */
	if (r == 5 && g == 6 && b == 5 && a == 0) return 0x8D62;    /* GL_RGB565 */
	if (r == 8 && g == 8 && b == 8 && a == 0) return 0x8051;    /* GL_RGB8 */
	if (r == 8 && g == 8 && b == 8 && a == 8) return 0x8058;    /* GL_RGBA8 */
	return 0;
}

/* Choose a shared supported count; never round color and depth independently */
static inline int android_gpu_common_samples(const int *color, int color_count,
                                             const int *depth, int depth_count, int requested)
{
	int result = 0;
	if (requested < 2) return 0;
	for (int i = 0; i < color_count; ++i) {
		if (color[i] < requested || (result && color[i] >= result)) continue;
		for (int j = 0; j < depth_count; ++j)
			if (color[i] == depth[j]) result = color[i];
	}
	return result;
}
#endif
