#include "lighting.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>

// Scalar vector/side adapter only; projection and edge-selection code is extracted
struct CFloatVector {
	xl_compat::LightPoint p{};
	CFloatVector operator-(const CFloatVector &b) const
	{
		return { { p[0] - b.p[0], p[1] - b.p[1], p[2] - b.p[2] } };
	}
	CFloatVector operator+(const CFloatVector &b) const
	{
		return { { p[0] + b.p[0], p[1] + b.p[1], p[2] + b.p[2] } };
	}
	CFloatVector operator*(float value) const
	{
		return { { p[0] * value, p[1] * value, p[2] * value } };
	}
	void operator*=(float value)
	{
		*this = *this * value;
	}
	void operator+=(const CFloatVector &b)
	{
		*this = *this + b;
	}
	void Assign(const CFloatVector &b)
	{
		*this = b;
	}
	static float Dot(const CFloatVector &a, const CFloatVector &b)
	{
		return a.p[0] * b.p[0] + a.p[1] * b.p[1] + a.p[2] * b.p[2];
	}
	float SqrMag() const
	{
		return Dot(*this, *this);
	}
	float Mag() const
	{
		return std::sqrt(SqrMag());
	}
	static float Dist(const CFloatVector &a, const CFloatVector &b)
	{
		return (a - b).Mag();
	}
};
using CFloatVector3 = CFloatVector;
struct CSide {
	uint16_t m_vertices[6]{}, m_corners[4]{ 0, 1, 2, 3 };
	int m_nFaces = 2, m_nCorners = 4;
	CFloatVector m_fNormals[2];
};
static CFloatVector reference_vertices[4];
static struct Segment {
	CSide side;
	CSide *Side(uint8_t)
	{
		return &side;
	}
} segment;
#define FVERTICES      reference_vertices
#define SEGMENT(index) (&segment)
#define Swap           std::swap
#include "xl_reference_face_point.h"
#undef Swap
#undef SEGMENT
#undef FVERTICES

int main()
{
	try {
		unsigned portal_cases = 0;
		std::array<std::uint16_t, 3> ids{ 3, 19, 500 };
		do {
			const std::array<xl_compat::LightPoint, 3> points{ xl_compat::LightPoint{ 0.1f, 2.2f, -3.3f }, { 4.4f, -5.5f, 6.6f }, { -7.7f, 8.8f, 9.9f } };
			uint16_t sorted[4];
			const auto flip = SortVertsForNormal(ids[0], ids[1], ids[2], 0xffff, sorted);
			std::array<xl_compat::LightPoint, 3> ordered;
			for (unsigned i = 0; i < 3; ++i) ordered[i] = points[std::find(ids.begin(), ids.end(), sorted[i]) - ids.begin()];
			auto expected = xl_compat::geometry_triangle_normal(ordered[0], ordered[1], ordered[2]);
			if (flip)
				for (auto &coordinate : expected) coordinate = -coordinate;
			if (xl_compat::geometry_portal_normal(points, ids) != expected) throw std::runtime_error("portal normal ordering differs from pinned source");
			++portal_cases;
		} while (std::next_permutation(ids.begin(), ids.end()));
		std::cout << portal_cases << " portal triangle orderings match pinned source\n";
		unsigned checked = 0;
		for (int shape = 0; shape < 5; ++shape) {
			xl_compat::GeometryLightFace face;
			face.corners = { xl_compat::LightPoint{ 0, 0, 0 }, { 2, 0, 0 }, { 2, 2, 0 }, { 0, 2, shape == 1 || shape == 2 ? 2.0f : 0.0f } };
			face.triangles = { std::array<unsigned, 3>{ 0, 1, 2 }, { 0, 2, 3 } };
			face.normals = { xl_compat::LightPoint{ 0, 0, 1 }, { 0, 0, 1 } };
			if (shape == 1 || shape == 2) {
				const float unit = 1 / std::sqrt(3.0f);
				face.normals[1] = { unit, -unit, unit };
			}
			if (shape == 2) {
				std::swap(face.triangles[0], face.triangles[1]);
				std::swap(face.normals[0], face.normals[1]);
			}
			if (shape == 3) face.normals = {};
			// CSide::SetupVertexList gives a compiled quad one face, not two
			if (shape == 4) face.triangle_count = 1;
			segment.side.m_nFaces = static_cast<int>(face.triangle_count);
			for (unsigned i = 0; i < 4; ++i) reference_vertices[i].p = face.corners[i];
			for (unsigned i = 0; i < 2; ++i) {
				segment.side.m_fNormals[i].p = face.normals[i];
				for (unsigned j = 0; j < 3; ++j) segment.side.m_vertices[i * 3 + j] = static_cast<uint16_t>(face.triangles[i][j]);
			}
			for (float x : { -2.0f, -0.000001f, 0.0f, 0.5f, 1.0f, 2.0f, 4.0f })
				for (float y : { -2.0f, 0.0f, 0.5f, 1.5f, 2.0f, 4.0f })
					for (float z : { -3.0f, -0.0005f, 0.0f, 0.0005f, 1.0f, 3.0f }) {
						const xl_compat::LightPoint point{ x, y, z };
						CFloatVector hit;
						DistToFace({ point }, 0, 0, &hit);
						const auto actual = xl_compat::geometry_light_point(face, point);
						for (unsigned c = 0; c < 3; ++c)
							if (!std::isfinite(actual[c]) || std::abs(actual[c] - hit.p[c]) > 1e-6f)
								throw std::runtime_error("emitting-face point differs from pinned source");
						++checked;
					}
		}
		std::cout << checked << " face points match pinned scalar reference; runtime face compilation unqualified\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
