#include "lighting.h"
#include <iostream>
#include <stdexcept>

// Scalar storage/arithmetic adapter; center and intersection control flow are extracted
using fix = std::int32_t;
#define I2X(value)   ((value) * 65536)
#define FixDiv(a, b) static_cast<fix>((b) ? double(a) / double(b) * 65536.0 : 1)
struct CFloatVector {
	xl_compat::LightPoint p{};
	void SetZero()
	{
		p = {};
	}
	void operator+=(const CFloatVector &other)
	{
		for (unsigned i = 0; i < 3; ++i) p[i] += other.p[i];
	}
	void operator/=(float divisor)
	{
		for (auto &value : p) value /= divisor;
	}
};
struct CFixVector {
	xl_compat::FixedLightPoint p{};
	CFixVector operator+(const CFixVector &other) const
	{
		return { { p[0] + other.p[0], p[1] + other.p[1], p[2] + other.p[2] } };
	}
	CFixVector operator-(const CFixVector &other) const
	{
		return { { p[0] - other.p[0], p[1] - other.p[1], p[2] - other.p[2] } };
	}
	void operator*=(fix factor)
	{
		for (auto &value : p) value = static_cast<fix>(double(factor) / 65536.0 * value);
	}
	void Assign(const CFloatVector &other)
	{
		for (unsigned i = 0; i < 3; ++i) p[i] = static_cast<fix>(other.p[i] * 65536);
	}
	static fix Dot(const CFixVector &a, const CFixVector &b)
	{
		return static_cast<fix>((double(a.p[0]) * b.p[0] + double(a.p[1]) * b.p[1] + double(a.p[2]) * b.p[2]) / 65536.0);
	}
};
static CFloatVector float_vertices[4];
static CFixVector fixed_vertices[4];
struct CSide {
	int m_nCorners = 4, m_corners[4]{ 0, 1, 2, 3 };
	CFixVector m_vCenter, m_normals[3];
	int FaceCount() const
	{
		return 2;
	}
	void ComputeCenter();
};
#define FVERTICES float_vertices
#define VERTICES  fixed_vertices
#include "xl_reference_side_center.h"
#undef FVERTICES
#undef VERTICES
#undef FixDiv
#undef I2X

int main()
{
	try {
		unsigned checked = 0;
		const std::array<xl_compat::FixedLightPoint, 5> normals{ xl_compat::FixedLightPoint{ 0, 0, 65536 }, { 0, 0, -65536 }, { 37837, -37837, 37837 }, { 23171, 46341, -40133 }, { 0, 0, 0 } };
		for (float shift : { -1000.125f, -0.0000152587890625f, 0.0f, 17.25f })
			for (float height : { -8.0f, -0.5f, 0.0f, 0.0000152587890625f, 0.5f, 2.0f, 8.0f })
				for (const auto &first : normals)
					for (const auto &second : normals) {
						const std::array<xl_compat::LightPoint, 4> corners{ xl_compat::LightPoint{ shift, shift, shift }, { shift + 2, shift, shift }, { shift + 2, shift + 2, shift }, { shift, shift + 2, shift + height } };
						for (unsigned i = 0; i < 4; ++i) {
							float_vertices[i].p = corners[i];
							fixed_vertices[i].Assign(float_vertices[i]);
						}
						CSide side;
						side.m_normals[0].p = first;
						side.m_normals[1].p = second;
						for (unsigned i = 0; i < 3; ++i) side.m_normals[2].p[i] = (first[i] + second[i]) / 2;
						side.ComputeCenter();
						if (xl_compat::geometry_side_center(corners, { first, second }, fixed_vertices[0].p) != side.m_vCenter.p)
							throw std::runtime_error("side center differs from pinned source");
						++checked;
					}
		// Large original fixed coordinates retain bits that the float mesh cannot
		for (int low_bits : { -3, -1, 1, 3 }) {
			CSide side;
			for (unsigned i = 0; i < 4; ++i) {
				fixed_vertices[i].p = { -32938036 + low_bits, int(i & 1) * 65536, int(i / 2) * 65536 };
				for (unsigned axis = 0; axis < 3; ++axis) float_vertices[i].p[axis] = float(fixed_vertices[i].p[axis]) / 65536.0f;
			}
			for (auto &normal : side.m_normals) normal.p = { 65536, 0, 0 };
			side.ComputeCenter();
			std::array<xl_compat::LightPoint, 4> corners;
			for (unsigned i = 0; i < 4; ++i) corners[i] = float_vertices[i].p;
			if (xl_compat::geometry_side_center(corners, { side.m_normals[0].p, side.m_normals[1].p }, fixed_vertices[0].p) != side.m_vCenter.p)
				throw std::runtime_error("side center lost original fixed plane vertex precision");
			++checked;
		}
		std::cout << checked << " side centers match pinned source; supplied fixed normals are not compiler certification\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
