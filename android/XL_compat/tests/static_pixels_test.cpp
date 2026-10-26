#include "../../app/src/main/cpp/shared/xl_static_texture_pixels.h"
#include <array>
#include <vector>
#include <stdexcept>
#include <iostream>
static void require(bool value)
{
	if (!value) throw std::runtime_error("Static pixel classification contract failed");
}
int main()
{
	std::vector<unsigned char> rgba(8 * 8 * 4, 255);
	for (size_t i = 0; i < 64; ++i) {
		rgba[i * 4] = 200;
		rgba[i * 4 + 1] = 40;
		rgba[i * 4 + 2] = 20;
	}
	rgba[0] = 120;
	rgba[1] = 88;
	rgba[2] = 128;
	rgba[7 * 4 + 3] = 0; // Ordinary cutout in another output cell
	dxx_xl_static_pixels out{};
	require(dxx_xl_fit_static_pixels(rgba.data(), 8, 8, 4, 4, &out));
	require(out.width == 4 && out.height == 4 && out.super && out.mask == out.data + 64);
	int holes = 0, cutouts = 0;
	for (size_t i = 0; i < 16; ++i) {
		holes += out.mask[i] == 255;
		cutouts += out.data[i * 4 + 3] == 0;
		require(out.data[i * 4] == 200 && out.data[i * 4 + 1] == 40 && out.data[i * 4 + 2] == 20);
	}
	require(holes == 1 && cutouts == 2 && out.mask[0] == 255 && out.mask[3] == 0);
	dxx_xl_static_pixels mip{};
	require(dxx_xl_fit_static_coverage(out.data, out.mask, 4, 4, 4, 1, &mip) && mip.super && mip.mask[0] == 255 && !mip.data[3]);
	free(mip.data);
	free(out.data);
	rgba[63 * 4 + 3] = 128; // Rejection must inspect pixels outside the first fitted cell
	require(!dxx_xl_fit_static_pixels(rgba.data(), 8, 8, 4, 4, &out) && !out.data && !out.mask && !out.super);
	for (const auto &shape : { std::array<int, 4>{ 0, 1, 4, 4 }, { 4, 8, 4, 4 }, { 16385, 1, 4, 4 }, { 16384, 16384, 4, 4 }, { 8, 8, 2, 4 }, { 8, 8, 4, 0 }, { 8, 8, 4, 16385 } })
		require(!dxx_xl_fit_static_pixels(rgba.data(), shape[0], shape[1], shape[2], shape[3], &out));
	unsigned char rgb[] = { 120, 88, 128 };
	require(dxx_xl_fit_static_pixels(rgb, 1, 1, 3, 1, &out) && out.super && !out.data[3] && out.mask[0] == 255);
	free(out.data);
	rgb[0] = 121;
	require(dxx_xl_fit_static_pixels(rgb, 1, 1, 3, 1, &out) && !out.super && out.data[3] == 255 && !out.mask[0]);
	free(out.data);
	unsigned char key_alpha[] = { 120, 88, 128, 104 };
	require(dxx_xl_fit_static_pixels(key_alpha, 1, 1, 4, 1, &out) && out.super && !out.data[3]);
	free(out.data);
	key_alpha[0] = 124;
	require(!dxx_xl_fit_static_pixels(key_alpha, 1, 1, 4, 1, &out));
	unsigned char soft[] = { 200, 40, 20, 128, 255, 0, 255, 0, 120, 88, 128, 255, 200, 40, 20, 255 };
	require(dxx_xl_fit_soft_coverage(soft, nullptr, 2, 2, 4, 2, 0, &out));
	require(out.super && out.data[0] == 100 && out.data[3] == 128 && out.data[4] == 0 && out.mask[2] == 255);
	require(dxx_xl_fit_soft_coverage(out.data, out.mask, 2, 2, 4, 1, 1, &mip));
	require(mip.data[0] == 75 && mip.data[1] == 15 && mip.data[3] == 95 && mip.mask[0] == 255);
	free(mip.data);
	free(out.data);
	require(!dxx_xl_fit_soft_coverage(soft, nullptr, 2, 2, 4, 1, 1, &out) && !out.data);
	std::cout << "Static source masks: sparse key, ordinary cutout, before-fit rejection, allocation bounds and state reset passed\n";
}
