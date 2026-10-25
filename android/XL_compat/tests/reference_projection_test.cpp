#include "lighting.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cstring>

// Adapter supplies the matrix result: this oracle covers projection arithmetic,
// not camera construction or matrix multiplication
using fix = std::int32_t;
constexpr unsigned PF_PROJECTED = 1, CC_BEHIND = 128;
struct CFloatVector3 {
	struct {
		struct {
			float x, y, z;
		} coord;
	} v;
};
struct CTransformation {
	struct {
		struct {
			CFloatVector3 result;
			CFloatVector3 operator*(const CFloatVector3 &) const
			{
				return result;
			}
		} projection;
	} m_info;
};
struct CCanvas {
	static float fCanvW2, fCanvH2;
};
float CCanvas::fCanvW2 = 512, CCanvas::fCanvH2 = 512;
struct CRenderPoint {
	uint8_t m_flags = 0, m_codes = 0;
	struct {
		fix x = 0, y = 0;
	} m_screen;
	uint8_t Project(CTransformation &, CFloatVector3 &);
};
using std::fabs;
#include "xl_reference_projection.h"

int main(int argc, char **argv)
{
	try {
		if (argc == 2 && std::strcmp(argv[1], "--probe-singular") == 0) {
			// Diagnostic only: report the host result of the reference's undefined
			// float-to-integer conversions; do not treat this as portable C++ truth
			volatile float zero = 0;
			for (float depth : { float(zero), -float(zero), std::numeric_limits<float>::min() })
				for (float coordinate : { -1.0f, 0.0f, 1.0f }) {
					CFloatVector3 view{ { { 0, 0, depth } } };
					CTransformation transform;
					transform.m_info.projection.result = { { { coordinate, coordinate, 0 } } };
					CRenderPoint reference;
					reference.Project(transform, view);
					std::cout << "depth=" << depth << " xy=" << coordinate << " screen=" << reference.m_screen.x << ',' << reference.m_screen.y << '\n';
				}
			return 0;
		}
		unsigned cases = 0;
		for (float extent : { 240.0f, 512.0f, 960.0f })
			for (float depth : { -100.0f, -1.0f, -0.03125f, 0.03125f, 1.0f, 100.0f })
				for (int x = -8; x <= 8; ++x)
					for (int y = -8; y <= 8; ++y) {
						CCanvas::fCanvW2 = extent;
						CCanvas::fCanvH2 = extent * 0.75f;
						CFloatVector3 view{ { { 0, 0, depth } } };
						CTransformation transform;
						transform.m_info.projection.result = { { { float(x) * 0.37f, float(y) * 0.29f, 0 } } };
						CRenderPoint reference;
						reference.Project(transform, view);
						const auto actual = xl_compat::geometry_screen_point({ float(x) * 0.37f, float(y) * 0.29f }, depth, extent, extent * 0.75f);
						if (actual.x != reference.m_screen.x || actual.y != reference.m_screen.y || actual.behind != (depth < 0))
							throw std::runtime_error("screen projection differs from pinned reference");
						++cases;
					}
		// Defined compatibility policy, independently asserted on every host
		for (float depth : { 0.0f, -0.0f, std::numeric_limits<float>::min() })
			for (float coordinate : { -1.0f, 0.0f, 1.0f }) {
				const auto actual = xl_compat::geometry_screen_point({ coordinate, coordinate }, depth, 512, 512);
				const auto expected = depth != 0 && coordinate == 0 ? 512 : std::numeric_limits<std::int32_t>::min();
				if (actual.x != expected || actual.y != expected || actual.behind)
					throw std::runtime_error("integer-indefinite projection policy differs");
			}
		for (float depth : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
			bool rejected = false;
			try {
				(void) xl_compat::geometry_screen_point({ 1, 1 }, depth, 512, 512);
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			if (!rejected) throw std::runtime_error("undefined projection admitted");
		}
		std::cout << cases << " reference screen projections passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
