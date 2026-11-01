#ifndef DXX_XL_CAMERA_ALIGNMENT_H
#define DXX_XL_CAMERA_ALIGNMENT_H
#include <array>
#include <cmath>
#include <stdexcept>

namespace xl_compat
{
struct CameraUV {
	float u = 0, v = 0;
};
// Pinned release CCamera::Align float-coordinate path used by renderlib.cpp
// The source fits to the wall unconditionally and caches until ReAlign
struct CameraAlignment {
	std::array<CameraUV, 6> coordinates{};
	bool aligned = false;
	void invalidate()
	{
		aligned = false;
	}
	void align(const std::array<CameraUV, 6> &input, bool triangles, bool diagonal13,
	           bool mirror, bool have_buffer, bool hires, int canvas_width, int canvas_height,
	           int texture_width, int texture_height)
	{
		if (aligned) return;
		if (canvas_width <= 0 || canvas_height <= 0 || texture_width <= 0 || texture_height <= 0)
			throw std::runtime_error("Camera alignment requires positive dimensions");
		constexpr float epsilon = .01f;
		const int i2 = triangles && diagonal13 ? 4 : 2, i3 = triangles ? 5 : 3;
		const float delta = input[1].u - input[0].u;
		const bool left = delta > epsilon, right = delta < -epsilon;
		bool x_flip, y_flip;
		if (left) {
			y_flip = input[1].u - input[i2].u > epsilon;
			x_flip = input[i3].v - input[1].v < -epsilon;
		} else if (right) {
			y_flip = input[i3].u - input[0].u > epsilon;
			x_flip = input[1].v - input[i3].v < -epsilon;
		} else {
			x_flip = input[i2].u - input[0].u < -epsilon;
			y_flip = input[1].v - input[0].v > epsilon;
		}
		float du = std::fabs(input[i2].u - input[0].u), dv = std::fabs(input[1].v - input[0].v);
		if (du <= epsilon) du = 0;
		if (dv <= epsilon) dv = 0;
		if (have_buffer) {
			const int scale = hires ? 1 : 2;
			du = float(canvas_width) / float(texture_width) / scale;
			dv = float(canvas_height) / float(texture_height) / scale;
		}
		if (mirror) x_flip = !x_flip;
		std::array<CameraUV, 4> corners{ { { x_flip ? du : 0, y_flip ? dv : 0 },
			                               { x_flip ? du : 0, y_flip ? 0 : dv },
			                               { x_flip ? 0 : du, y_flip ? 0 : dv },
			                               { x_flip ? 0 : du, y_flip ? dv : 0 } } };
		const int rotation = left ? 1 : right ? 3
		                                      : 0;
		for (int i = 0; i < 4; ++i) coordinates[i] = corners[(i + rotation) % 4];
		if (triangles) {
			coordinates[5] = coordinates[3];
			coordinates[4] = coordinates[2];
			coordinates[3] = coordinates[diagonal13 ? 1 : 0];
			if (diagonal13) coordinates[2] = coordinates[5];
		}
		aligned = true;
	}
};
} // namespace xl_compat
#endif
