#include "lighting.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using fix = std::int32_t;
struct Options {
	struct {
		int nMathFormat = 2;
	} render;
} options;
Options *gameOpts = &options;
struct tQuadInt {
	std::uint32_t low;
	std::int32_t high;
};
// This oracle exercises only math format 2; fail if legacy code is reached
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected legacy magnitude branch");
}
std::uint32_t QuadSqrt(std::uint32_t, std::int32_t)
{
	throw std::runtime_error("Unexpected legacy square root");
}
#define FixDiv(a, b) (fix)((b) ? (double(a) / double(b) * 65536.0) : 1)
struct CFixVector {
	struct {
		struct {
			fix x, y, z;
		} coord;
	} v;
	fix Mag() const;
	static const fix Normalize(CFixVector &v);
};
using std::sqrt;
#include "xl_reference_magnitude.h"

int main()
{
	try {
		unsigned cases = 0;
		auto check = [&](const xl_compat::FixedLightPoint &point) {
			CFixVector reference{ { { point[0], point[1], point[2] } } };
			if (xl_compat::geometry_default_magnitude(point) != reference.Mag())
				throw std::runtime_error("Default magnitude differs from reference");
			CFixVector::Normalize(reference);
			if (xl_compat::geometry_default_direction(point) != xl_compat::FixedLightPoint{ reference.v.coord.x, reference.v.coord.y, reference.v.coord.z })
				throw std::runtime_error("Default normalization differs from reference");
			++cases;
		};
		for (fix x : { -65536, -3, -1, 0, 1, 3, 65536 })
			for (fix y : { -65536, -4, -1, 0, 1, 4, 65536 })
				for (fix z : { -65536, -5, -1, 0, 1, 5, 65536 }) check({ x, y, z });
		std::uint32_t state = 0x6731;
		for (unsigned i = 0; i < 20000; ++i) {
			xl_compat::FixedLightPoint point;
			for (auto &axis : point) {
				state = state * 1664525u + 1013904223u;
				axis = static_cast<fix>(state % 2000000001u) - 1000000000;
			}
			check(point);
		}
		check({ std::numeric_limits<fix>::max(), 0, 0 });
		bool rejected = false;
		try {
			xl_compat::geometry_default_magnitude({ std::numeric_limits<fix>::max(), std::numeric_limits<fix>::max(), 0 });
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Out-of-range magnitude admitted");
		std::cout << cases << " default magnitude/normalization reference cases passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
