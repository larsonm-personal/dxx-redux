#include "lighting.h"
#include <iostream>
#include <stdexcept>

using fix = std::int32_t;
struct CFixVector {
	struct {
		struct {
			fix x, y, z;
		} coord;
	} v;
	static CFixVector Create(fix x, fix y, fix z)
	{
		return { { { x, y, z } } };
	}
	static const CFixVector Cross(const CFixVector &a, const CFixVector &b);
};
#define mul64(a, b) (static_cast<int64_t>(a) * static_cast<int64_t>(b))
#include "xl_reference_fixed_cross.h"
#undef mul64

int main()
{
	try {
		std::uint32_t seed = 0x12345678;
		for (unsigned test = 0; test < 10000; ++test) {
			xl_compat::FixedLightPoint a, b;
			for (unsigned axis = 0; axis < 3; ++axis) {
				seed = seed * 1664525u + 1013904223u;
				a[axis] = static_cast<fix>((seed >> 8) & 0x7ffff) - 262144;
				seed = seed * 1664525u + 1013904223u;
				b[axis] = static_cast<fix>((seed >> 8) & 0x7ffff) - 262144;
			}
			const auto expected = CFixVector::Cross(CFixVector::Create(a[0], a[1], a[2]), CFixVector::Create(b[0], b[1], b[2]));
			if (xl_compat::geometry_fixed_cross(a, b) != xl_compat::FixedLightPoint{ expected.v.coord.x, expected.v.coord.y, expected.v.coord.z })
				throw std::runtime_error("fixed cross differs from the returning XL overload");
		}
		std::cout << "10000 fixed cross products match the pinned returning overload\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
