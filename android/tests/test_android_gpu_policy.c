#include <assert.h>
#include "android_gpu_policy.h"

int main(void)
{
	assert(android_gpu_has_extension("GL_EXT_other GL_EXT_texture_filter_anisotropic", "GL_EXT_texture_filter_anisotropic"));
	assert(android_gpu_has_extension("GL_EXT_texture_filter_anisotropic GL_EXT_other", "GL_EXT_texture_filter_anisotropic"));
	assert(!android_gpu_has_extension("GL_EXT_texture_filter_anisotropic2", "GL_EXT_texture_filter_anisotropic"));
	assert(!android_gpu_has_extension("XGL_EXT_texture_filter_anisotropic", "GL_EXT_texture_filter_anisotropic"));
	assert(!android_gpu_has_extension(NULL, "GL_EXT_texture_filter_anisotropic"));
	assert(android_gpu_color_format(10, 10, 10, 2) == 0x8059);
	assert(android_gpu_color_format(5, 6, 5, 0) == 0x8D62);
	assert(android_gpu_color_format(8, 8, 8, 0) == 0x8051);
	assert(android_gpu_color_format(8, 8, 8, 8) == 0x8058);
	/* Previously guessed RGBA8/RGB565; these must not reach a mismatched resolve */
	assert(!android_gpu_color_format(4, 4, 4, 4));
	assert(!android_gpu_color_format(5, 5, 5, 1));
	assert(!android_gpu_color_format(16, 16, 16, 16));
	assert(!android_gpu_color_format(0, 0, 0, 0));
	const int color[] = { 8, 4, 2 };
	const int depth[] = { 8, 4 };
	assert(android_gpu_common_samples(color, 3, depth, 2, 2) == 4);
	assert(android_gpu_common_samples(color, 3, depth, 2, 4) == 4);
	assert(android_gpu_common_samples(color, 3, depth, 2, 8) == 8);
	assert(!android_gpu_common_samples(color, 3, depth, 2, 16));
	assert(!android_gpu_common_samples(color, 3, depth, 2, 0));
	assert(!android_gpu_common_samples(color, 0, depth, 2, 2));
	const int incompatible[] = { 16 };
	assert(!android_gpu_common_samples(color, 3, incompatible, 1, 2));
	return 0;
}
