// Included by transparency_scene.cpp after native declarations
#include "../sprite_render.h"
namespace
{
std::array<float, 3> powerup_bitmap_color(grs_bitmap &bitmap)
{
	if (!bitmap.bm_data || bitmap.bm_w <= 0 || bitmap.bm_h <= 0) throw std::runtime_error("XL powerup bitmap is unavailable");
	if (bitmap.bm_type == BM_RGBA) {
		std::array<float, 3> color{};
		for (unsigned axis = 0; axis < 3; ++axis)
			color[axis] = float(std::lround(double(bitmap.avg_color_rgb[axis]) * 255 / 65536)) / 255.f;
		if (color[0] + color[1] + color[2] == 0) color = { 1, 1, 1 };
		return color;
	}
	const unsigned char *pixels = bitmap.bm_data;
	unsigned stride = bitmap.bm_rowsize;
	std::vector<unsigned char> decoded;
	if (!bitmap.avg_color && (bitmap.bm_flags & BM_FLAG_RLE)) {
		decoded.resize(std::size_t(bitmap.bm_w) * bitmap.bm_h);
		const unsigned row_bytes = bitmap.bm_flags & BM_FLAG_RLE_BIG ? 2 : 1;
		auto *row = bitmap.bm_data + 4 + bitmap.bm_h * row_bytes;
		for (int y = 0; y < bitmap.bm_h; ++y) {
			gr_rle_decode(row, decoded.data() + std::size_t(y) * bitmap.bm_w);
			const auto *length = bitmap.bm_data + 4 + y * row_bytes;
			row += length[0] + (row_bytes == 2 ? unsigned(length[1]) * 256 : 0);
		}
		pixels = decoded.data();
		stride = bitmap.bm_w;
	}
	return xl_compat::sprite_bitmap_color(bitmap.avg_color, pixels, bitmap.bm_w, bitmap.bm_h, stride, gr_current_pal);
}
xl_compat::ParticleQuad powerup_billboard(const vms_vector &position, fix width, fix height, const std::array<float, 4> &color, xl_compat::ParticleBlend blend)
{
	const std::array<float, 3> delta{ float(std::int64_t(position.x) - View_position.x) / 65536.f, float(std::int64_t(position.y) - View_position.y) / 65536.f, float(std::int64_t(position.z) - View_position.z) / 65536.f };
	const auto dot = [&](const vms_vector &axis) {
		return delta[0] * (float(axis.x) / 65536.f) + delta[1] * (float(axis.y) / 65536.f) + delta[2] * (float(axis.z) / 65536.f);
	};
	const std::array<float, 3> center{ dot(View_matrix.rvec), dot(View_matrix.uvec), -dot(View_matrix.fvec) };
	// Redux folds window/zoom scale into View_matrix, unlike source RenderQuad
	const float w = (float(width) / 65536.f) * (float(Matrix_scale.x) / 65536.f);
	const float h = (float(height) / 65536.f) * (float(Matrix_scale.y) / 65536.f);
	xl_compat::ParticleQuad quad{};
	quad.blend = blend;
	const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
	for (unsigned i = 0; i < 4; ++i) {
		quad.vertices[i].position = { center[0] + corners[i][0] * w, center[1] + corners[i][1] * h, center[2] };
		quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
		quad.vertices[i].color = color;
	}
	return quad;
}
} // namespace
