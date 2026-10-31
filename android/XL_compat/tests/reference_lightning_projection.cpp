#include "lightning_render.h"
#include <cstring>
#include <iostream>
#include <limits>

namespace lightning_projection_reference
{
struct CFloatVector3 {
	struct {
		struct {
			float x, y, z;
		} coord;
	} v;
	void Set(float x, float y, float z)
	{
		v.coord = { x, y, z };
	}
	static const float Dot(const CFloatVector3 &, const CFloatVector3 &);
};
struct CFloatVector {
	float values[4];
	CFloatVector3 *XYZ()
	{
		return reinterpret_cast<CFloatVector3 *>(values);
	}
};
struct CFloatMatrix {
	union {
		float vec[16];
		struct {
			CFloatVector r, u, f, w;
		} dir;
	} m{};
	void Flip();
	const CFloatVector3 operator*(const CFloatVector3 &);
};
template <class T>
void Swap(T &a, T &b)
{
	std::swap(a, b);
}
struct {
	struct {
		CFloatMatrix projection;
	} m_info;
} transformation;
struct CCanvas {
	static float fCanvW2, fCanvH2;
};
float CCanvas::fCanvW2, CCanvas::fCanvH2;
struct tScreenPos {
	int32_t x, y;
};
using fix = int32_t;
constexpr uint8_t PF_PROJECTED = 1, CC_BEHIND = 2;
#include "xl_reference_lightning_projection.h"
} // namespace lightning_projection_reference

void lightning_projection_controls()
{
	using namespace lightning_projection_reference;
	unsigned cases = 0, old_mismatches = 0;
	for (const auto canvas : { std::array<int, 2>{ 64, 64 }, std::array<int, 2>{ 481, 273 }, std::array<int, 2>{ 1920, 1080 } })
		for (float aspect : { .5f, 1.f, 1.7777778f })
			for (float shift : { -.2f, 0.f, .3f }) {
				const std::array<float, 16> projection{ 1.f / aspect, 0, 0, 0, 0, 1.25f, 0, 0, shift, -shift / 2, -1.0004f, -1, 17, -9, -2.0004f, 0 };
				std::copy(projection.begin(), projection.end(), transformation.m_info.projection.m.vec);
				transformation.m_info.projection.Flip();
				CCanvas::fCanvW2 = float(canvas[0]) / 2;
				CCanvas::fCanvH2 = float(canvas[1]) / 2;
				for (float x : { -13.25f, 0.f, 8.375f })
					for (float y : { -7.5f, 0.f, 15.125f })
						for (float z : { -4999.f, -1.f, -.03125f, .03125f, 1.f, 4999.f }) {
							CFloatVector3 point;
							point.Set(x, y, z);
							tScreenPos expected{};
							if (ProjectPoint(point, expected, 0, 0) != PF_PROJECTED) throw std::runtime_error("Pinned lightning projection did not project");
							const auto actual = xl_compat::lightning_screen_point({ x, y, z }, projection, canvas[0], canvas[1]);
							if (actual != std::array<int, 2>{ expected.x, expected.y }) throw std::runtime_error("Lightning viewport point differs from pinned projection");
							const auto old = xl_compat::lightning_clip_vertex({ x, y, -z }, {}, projection);
							old_mismatches += int(CCanvas::fCanvW2 * (1 + old.position[0] / std::abs(z))) != expected.x || int(CCanvas::fCanvH2 * (1 + old.position[1] / std::abs(z))) != expected.y;
							++cases;
						}
			}
	if (!old_mismatches) throw std::runtime_error("Projection control failed to distinguish the old MVP bounds");
	const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	for (const auto point : { xl_compat::LightningVertex{ 0, 0, 0 }, xl_compat::LightningVertex{ 1, 0, 1e-30f }, xl_compat::LightningVertex{ std::numeric_limits<float>::infinity(), 0, 1 } }) {
		bool rejected = false;
		try {
			(void) xl_compat::lightning_screen_point(point, identity, 64, 64);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Lightning projection accepted an undefined source integer conversion");
	}
	std::cout << "Pinned lightning projection: " << cases << " front/behind/near-plane/canvas cases passed; " << old_mismatches << " distinguish old MVP bounds\n";
}
