#include "../door_obstruction.h"
#include <array>
#include <iostream>
#include <stdexcept>
using fix = int;
constexpr int PLANE_DIST_TOLERANCE = 250;
#define ENTER(a, b)
#define RETVAL(value) return value;
struct CSegMasks {
	int m_valid = 0, m_side = 0, m_face = 0, m_center = 0;
};
struct CFixVector {
	std::array<int, 2> distances{};
	int DistToPlane(int normal, const CFixVector &) const
	{
		return distances[normal];
	}
};
struct CSide {
	int m_nFaces = 2;
	bool planar = true;
	CFixVector MinVertex() const
	{
		return {};
	}
	int Normal(int face) const
	{
		return face;
	}
	bool IsPlanar() const
	{
		return planar;
	}
	CSegMasks Masks(const CFixVector &, fix, int16_t, int16_t, bool);
};
#include "xl_reference_door_masks.h"
void door_masks_reference()
{
	int samples = 0;
	for (int faces : { 1, 2 })
		for (bool planar : { false, true })
			for (int radius : { 0, 1, 249, 250, 65536 }) {
				const std::array<int, 9> distances{ -radius - 251, -radius - 250, -radius - 249, -251, -250, -249, radius - 251, radius - 250, radius - 249 };
				for (int a : distances)
					for (int b : distances) {
						CSide side{ faces, planar };
						CFixVector point{ { a, b } };
						const bool expected = radius && side.Masks(point, radius, 1, 1, true).m_side;
						const bool actual = xl_compat::door_sphere_pokes(radius, unsigned(faces), !planar, [&](unsigned face) { return point.distances[face]; });
						if (actual != expected) throw std::runtime_error("Door sphere/plane source mismatch");
						++samples;
					}
			}
	std::cout << samples << " extracted-reference sphere/plane boundary comparisons passed\n";
}
