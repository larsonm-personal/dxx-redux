#include "particle_world.h"
#include "particle_world_receipt.h"
#include <fstream>
#include <iostream>

// These bridges execute the pinned vector functions in the shared particle adapter
int particle_source_plane_distance(const xl_compat::ParticlePoint &, const xl_compat::ParticlePoint &, const xl_compat::ParticlePoint &);
int particle_source_distance(const xl_compat::ParticlePoint &, const xl_compat::ParticlePoint &);
xl_compat::ParticlePoint particle_source_normal(const xl_compat::ParticlePoint &, const xl_compat::ParticlePoint &, const xl_compat::ParticlePoint &);
namespace particle_world_reference
{
using fix = std::int32_t;
struct CFloatVector {
	struct {
		struct {
			float x = 0, y = 0, z = 0, w = 0;
		} coord;
	} v;
	void SetZero()
	{
		v.coord = {};
	}
	CFloatVector &operator+=(const CFloatVector &other)
	{
		v.coord.x += other.v.coord.x;
		v.coord.y += other.v.coord.y;
		v.coord.z += other.v.coord.z;
		return *this;
	}
	CFloatVector &operator/=(float divisor)
	{
		v.coord.x /= divisor;
		v.coord.y /= divisor;
		v.coord.z /= divisor;
		return *this;
	}
	// SetupAsTriangles also writes float lighting normals; particle world queries never read them
	static CFloatVector Normal(const CFloatVector &, const CFloatVector &, const CFloatVector &)
	{
		return {};
	}
	void Neg()
	{
		v.coord.x = -v.coord.x;
		v.coord.y = -v.coord.y;
		v.coord.z = -v.coord.z;
	}
};
struct CFixVector {
	struct {
		struct {
			int x = 0, y = 0, z = 0;
		} coord;
	} v;
	CFixVector() = default;
	CFixVector(const xl_compat::ParticlePoint &p)
	{
		v.coord = { p[0], p[1], p[2] };
	}
	void Set(int x, int y, int z)
	{
		v.coord = { x, y, z };
	}
	CFixVector &operator-=(const CFixVector &other)
	{
		*this = *this - other;
		return *this;
	}
	const CFixVector &operator/=(fix);
	xl_compat::ParticlePoint point() const
	{
		return { v.coord.x, v.coord.y, v.coord.z };
	}
	CFixVector operator-(const CFixVector &other) const
	{
		return xl_compat::ParticlePoint{ v.coord.x - other.v.coord.x, v.coord.y - other.v.coord.y, v.coord.z - other.v.coord.z };
	}
	void Neg()
	{
		v.coord.x = -v.coord.x;
		v.coord.y = -v.coord.y;
		v.coord.z = -v.coord.z;
	}
	CFixVector &Assign(const CFloatVector &);
	static CFixVector Normal(const CFixVector &a, const CFixVector &b, const CFixVector &c)
	{
		return particle_source_normal(a.point(), b.point(), c.point());
	}
	static fix Dot(const CFixVector &a, const CFixVector &b)
	{
		return particle_source_plane_distance(a.point(), b.point(), {});
	}
	fix DistToPlane(const CFixVector &normal, const CFixVector &anchor) const
	{
		return particle_source_plane_distance(point(), normal.point(), anchor.point());
	}
	static fix Dist(const CFixVector &a, const CFixVector &b)
	{
		return particle_source_distance(a.point(), b.point());
	}
};
struct CSegMasks {
	int m_valid = 0, m_face = 0, m_side = 0, m_center = 0;
	CSegMasks &operator|=(const CSegMasks &other)
	{
		m_face |= other.m_face;
		m_side |= other.m_side;
		m_center |= other.m_center;
		return *this;
	}
};
struct CSide {
	int m_nShape = 0, m_nFaces = 2, m_nMinVertex[2]{};
	int m_nType = 0;
	uint16_t m_corners[4]{}, m_vertices[6]{}, m_faceVerts[6]{};
	CFloatVector m_fNormals[2]{};
	CFixVector m_normals[2]{}, anchor;
	bool planar = true, compiled = false;
	CFixVector &Vertex(int);
	CFixVector MinVertex() const
	{
		return anchor;
	}
	CFixVector &Normal(int face)
	{
		return m_normals[face];
	}
	bool IsPlanar()
	{
		return compiled ? ComputedPlanar() : planar;
	}
	int FaceCount() const
	{
		return m_nFaces;
	}
	CSegMasks Masks(const CFixVector &, fix, int16_t, int16_t, bool = false);
	void SetupCorners(uint16_t *, uint16_t *);
	void SetupAsTriangles(bool, uint16_t *, uint16_t *);
	void SetupVertexList(uint16_t *, uint16_t *);
	void SetupFaceVertIndex();
	fix Height();
	bool ComputedPlanar();
	uint8_t Dist(const CFixVector &, fix &, int32_t, int16_t);
};
struct CSegment {
	int m_nShape = 0, m_function = 0;
	fix m_rads[2]{};
	CFixVector m_vCenter, m_extents[2];
	uint16_t m_vertices[8]{};
	CSide m_sides[6]{};
	int m_children[6]{};
	CFixVector Center() const
	{
		return m_vCenter;
	}
	CSegMasks Masks(const CFixVector &, fix);
	void ComputeCenter();
	void ComputeRads(fix);
	// Per-side render bounds are not consumed by containment or particle collision
	void ComputeSideRads() {}
};
std::vector<CSegment> segments;
CSegment *SEGMENT(int i)
{
	return &segments.at(std::size_t(i));
}
struct {
	CSegment *Buffer()
	{
		return segments.data();
	}
} SEGMENTS;
template <typename T>
struct CArray {
	std::vector<T> data;
	bool Create(int count, const char *)
	{
		if (count < 0 || count > 16000000) throw std::runtime_error("Source particle grid exceeds test storage limit");
		data.resize(std::size_t(count));
		return true;
	}
	void Clear()
	{
		std::fill(data.begin(), data.end(), T{});
	}
	void Destroy()
	{
		data.clear();
	}
	T *Buffer()
	{
		return data.empty() ? nullptr : data.data();
	}
	std::size_t Length() const
	{
		return data.size();
	}
	T &operator[](int i)
	{
		return data.at(std::size_t(i));
	}
};
struct tSegGridIndex {
	int32_t nIndex = 0;
	uint16_t nSegments = 0;
};
struct CSegmentGrid {
	CArray<tSegGridIndex> m_index;
	CArray<int16_t> m_segments;
	CFixVector m_vDim, m_vMin, m_vMax;
	int32_t m_nGridSize = 0;
	bool Create(int32_t, int32_t);
	void Destroy();
	int32_t GetSegList(CFixVector, int16_t *&);
	int32_t GridIndex(int32_t, int32_t, int32_t);
	bool Available()
	{
		return m_segments.Buffer() != nullptr;
	}
};
struct {
	struct {
		int nLastSegment = -1, nSegments = 0;
		CSegmentGrid grids[2];
		std::vector<CFixVector> vertices;
		struct {
			std::vector<int16_t> indices;
			int GetSegList(int16_t *&p)
			{
				p = indices.data();
				return int(indices.size());
			}
		} skybox;
		bool HaveSegmentGrid(int skybox)
		{
			return grids[skybox].Available();
		}
		int GetSegList(const CFixVector &point, int16_t *&list, int skybox)
		{
			return grids[skybox].GetSegList(point, list);
		}
	} segData;
} gameData;
std::vector<CFloatVector> float_vertices;
CFixVector &CSide::Vertex(int i)
{
	return gameData.segData.vertices.at(std::size_t(i));
}
struct CParticle {
	int m_nSegment = 0;
	CFixVector m_vPos;
	int32_t CollideWithWall(int32_t);
};
int nPartSeg[1] = { -1 }, nFaceCount[1][6]{}, bSidePokesOut[1][6]{};
CFixVector *wallNorm[1]{};
#define ENTER(a, b)
#define RETVAL(v)            return (v);
#define SEGMENT_SIDE_COUNT   6
#define SEGMENT_FUNC_SKYBOX  10
#define SIDE_SHAPE_TRIANGLE  1
#define PLANE_DIST_TOLERANCE 65
#include "xl_reference_particle_world.h"
#define VERTICES       gameData.segData.vertices
#define FVERTICES      float_vertices
#define SIDE_IS_QUAD   1
#define SIDE_IS_TRI_02 2
#define SIDE_IS_TRI_13 3
template <typename T>
T Min(T a, T b)
{
	return std::min(a, b);
}
template <typename T>
void Swap(T &a, T &b)
{
	std::swap(a, b);
}
#include "xl_reference_particle_world_setup.h"
#include "xl_reference_particle_grid.h"

void bind(const xl_compat::ParticleWorld &world)
{
	segments.clear();
	segments.resize(world.segments.size());
	gameData.segData.vertices.clear();
	gameData.segData.skybox.indices.clear();
	gameData.segData.nLastSegment = int(segments.size()) - 1;
	gameData.segData.nSegments = int(segments.size());
	for (std::size_t i = 0; i < segments.size(); ++i) {
		auto &to = segments[i];
		const auto &from = world.segments[i];
		to.m_vCenter = from.center;
		to.m_extents[0] = from.bounds[0];
		to.m_extents[1] = from.bounds[1];
		to.m_rads[0] = from.inner_radius;
		to.m_rads[1] = from.outer_radius;
		to.m_function = from.skybox ? SEGMENT_FUNC_SKYBOX : 0;
		if (from.skybox) gameData.segData.skybox.indices.push_back(int16_t(i));
		for (unsigned side = 0; side < 6; ++side) {
			auto &destination = to.m_sides[side];
			const auto &source = from.sides[side];
			destination.m_nShape = source.faces ? 0 : 2;
			destination.m_nFaces = int(source.faces);
			destination.anchor = source.anchor;
			destination.m_nMinVertex[0] = int(gameData.segData.vertices.size());
			gameData.segData.vertices.push_back({ source.anchor });
			destination.planar = !source.pokes_out;
			for (unsigned face = 0; face < 2; ++face) destination.m_normals[face] = source.normals[face];
			to.m_children[side] = source.child;
		}
	}
	for (int skybox : { 0, 1 }) {
		gameData.segData.grids[skybox].Destroy();
		if (!world.grids[skybox].cells.empty()) gameData.segData.grids[skybox].Create(skybox ? 80 : 40, skybox);
	}
}

void compile(const xl_compat::Inventory &source)
{
	segments.clear();
	segments.resize(source.segments.size());
	gameData.segData.vertices.clear();
	float_vertices.clear();
	gameData.segData.skybox.indices.clear();
	gameData.segData.nLastSegment = int(segments.size()) - 1;
	gameData.segData.nSegments = int(segments.size());
	for (const auto &point : source.vertices) {
		gameData.segData.vertices.emplace_back(point);
		CFloatVector v;
		v.v.coord = { point[0] / 65536.0f, point[1] / 65536.0f, point[2] / 65536.0f, 0 };
		float_vertices.push_back(v);
	}
	// The loader calls these source bodies with setup math format 0 for fixed normals,
	// then restores default format 2 before computing segment distances and radii
	for (std::size_t i = 0; i < segments.size(); ++i) {
		auto &segment = segments[i];
		const auto &input = source.segments[i];
		segment.m_function = input.function;
		if (input.function == SEGMENT_FUNC_SKYBOX) gameData.segData.skybox.indices.push_back(int16_t(i));
		for (unsigned j = 0; j < 8; ++j) {
			if (input.vertices[j] >= source.vertices.size()) throw std::runtime_error("Source particle-world vertex out of range");
			segment.m_vertices[j] = uint16_t(input.vertices[j]);
		}
		segment.ComputeCenter();
		fix inner = INT32_MAX;
		for (unsigned j = 0; j < 6; ++j) {
			auto &side = segment.m_sides[j];
			side.compiled = true;
			segment.m_children[j] = input.children[j];
			uint16_t slots[4];
			for (unsigned k = 0; k < 4; ++k) slots[k] = input.sides[j].corners[k];
			side.SetupCorners(segment.m_vertices, slots);
			side.SetupAsTriangles(input.children[j] < 0, segment.m_vertices, slots);
			side.anchor = side.Vertex(side.m_nMinVertex[0]);
			fix distance = 0;
			side.Dist(segment.Center(), distance, 0, int16_t(1 << j));
			inner = std::min(inner, distance);
		}
		segment.ComputeRads(inner);
	}
	gameData.segData.grids[0].Create(40, 0);
	gameData.segData.grids[1].Create(80, 1);
}

void compare_compiled(const xl_particle_world_receipt::Case &receipt)
{
	compile(receipt.source);
	for (unsigned skybox = 0; skybox < 2; ++skybox) {
		auto &source = gameData.segData.grids[skybox];
		const auto &native = receipt.world.grids[skybox];
		if (source.Available() != !native.cells.empty()) throw std::runtime_error("Particle grid availability differs");
		if (!source.Available()) continue;
		if (native.dimensions != source.m_vDim.point() || native.minimum != source.m_vMin.point() || native.size() != int64_t(source.m_index.Length())) throw std::runtime_error("Particle grid dimensions differ");
		for (int64_t i = 0; i < native.size(); ++i) {
			const auto &index = source.m_index[int(i)];
			const auto candidates = native.cells.find(i);
			if ((candidates == native.cells.end() ? 0 : candidates->second.size()) != index.nSegments) throw std::runtime_error("Particle grid candidate count differs");
			for (unsigned j = 0; j < index.nSegments; ++j)
				if (candidates->second[j] != source.m_segments[index.nIndex + int(j)]) throw std::runtime_error("Particle grid candidate ordering differs");
		}
	}
	for (std::size_t i = 0; i < segments.size(); ++i) {
		auto &expected = segments[i];
		const auto &actual = receipt.world.segments.at(i);
		const auto fail = [&](const char *field, int side = -1) { throw std::runtime_error("Native particle world differs from pinned setup: segment " + std::to_string(i) + " side " + std::to_string(side) + " " + field); };
		if (actual.center != expected.Center().point()) fail("center");
		if (actual.inner_radius != expected.m_rads[0]) fail("inner radius");
		if (actual.outer_radius != expected.m_rads[1]) fail("outer radius");
		for (unsigned bound = 0; bound < 2; ++bound)
			if (actual.bounds[bound] != expected.m_extents[bound].point()) fail("grid bounds");
		if (actual.skybox != (expected.m_function == SEGMENT_FUNC_SKYBOX)) fail("skybox");
		for (unsigned j = 0; j < 6; ++j) {
			auto &side = expected.m_sides[j];
			const auto &native = actual.sides[j];
			if (native.faces != unsigned(side.m_nFaces)) fail("face count", int(j));
			if (native.child != expected.m_children[j]) fail("child", int(j));
			if (native.anchor != side.MinVertex().point()) fail("anchor", int(j));
			if (native.pokes_out == side.IsPlanar()) fail("planarity", int(j));
			for (unsigned face = 0; face < 2; ++face)
				if (native.normals[face] != side.Normal(int(face)).point()) fail("fixed normal", int(j));
		}
	}
	for (const auto &query : receipt.queries) {
		if (FindSegByPosExhaustive({ query.position }, query.skybox, query.start) != query.located) throw std::runtime_error("Native particle world lookup differs from compiled source");
		CParticle particle;
		particle.m_nSegment = query.start;
		particle.m_vPos = query.position;
		nPartSeg[0] = -1;
		wallNorm[0] = nullptr;
		const bool hit = particle.CollideWithWall(0) != 0;
		if (hit != query.hit || particle.m_nSegment != query.after || (hit && (!wallNorm[0] || wallNorm[0]->point() != query.normal))) throw std::runtime_error("Native particle world collision differs from compiled source");
	}
}
} // namespace particle_world_reference

void compare_particle_world_receipt(const char *path)
{
	std::ifstream input(path);
	std::string token;
	unsigned version = 0;
	input >> token >> version;
	if (token != "XL_PARTICLE_WORLD" || version != 1) throw std::runtime_error("Invalid particle world receipt header");
	unsigned cases = 0, segments = 0, queries = 0;
	while (input >> token >> version) {
		if (token == "end") {
			if (version != cases || cases != 2) throw std::runtime_error("Incomplete stock/XL particle world receipt");
			input >> std::ws;
			if (!input.eof()) throw std::runtime_error("Trailing particle world receipt data");
			std::cout << "Pinned loaded particle world: " << cases << " stock/XL cases, " << segments << " compiled segments and " << queries << " native lookup/collision queries passed\n";
			return;
		}
		if (token != "case" || version != cases) throw std::runtime_error("Invalid particle world receipt case");
		const auto receipt = xl_particle_world_receipt::read(input);
		particle_world_reference::compare_compiled(receipt);
		segments += unsigned(receipt.world.segments.size());
		queries += unsigned(receipt.queries.size());
		++cases;
	}
	throw std::runtime_error("Missing particle world receipt terminator");
}

void particle_world_source_controls()
{
	using namespace particle_world_reference;
	xl_compat::ParticleWorld world;
	world.segments.resize(3);
	for (int i = 0; i < 3; ++i) {
		auto &segment = world.segments[i];
		segment.center = { i * 1310720, 0, 0 };
		for (unsigned axis = 0; axis < 3; ++axis) {
			segment.bounds[0][axis] = segment.center[axis] - 655360;
			segment.bounds[1][axis] = segment.center[axis] + 655360;
		}
		segment.inner_radius = 655360;
		segment.outer_radius = 1135116;
		segment.skybox = i == 2;
		for (unsigned side = 0; side < 6; ++side) {
			auto &s = segment.sides[side];
			const unsigned axis = side / 2;
			const int sign = (side & 1) ? -1 : 1;
			s.anchor = segment.center;
			s.anchor[axis] -= sign * 655360;
			s.normals[0][axis] = s.normals[1][axis] = sign * 65536;
			s.faces = 2;
		}
	}
	world.segments[0].sides[1].child = 1;
	world.segments[1].sides[0].child = 0;
	unsigned locations = 0, collisions = 0;
	for (int profile = 0; profile < 4; ++profile) {
		if (profile == 1) world.segments[0].inner_radius = 0;
		if (profile == 2) {
			world.build_grids();
			world.segments[0].sides[1].normals[1] = { -65536, 32768, 0 };
			world.segments[0].sides[1].pokes_out = true;
		}
		if (profile == 3) {
			world.segments[0].sides[1].pokes_out = false;
			world.segments[0].sides[3].faces = 1;
		}
		bind(world);
		for (int x : { -655426, -655425, -655424, -655360, 0, 655360, 655424, 655425, 655426, 1310720, 1966080, 2621440, 3276800 })
			for (int y : { -655426, -655425, -1, 0, 655360, 655425, 655426 })
				for (int z : { -655425, 0, 655425 })
					for (int start : { -3, -1, 0, 1, 2 }) {
						const xl_compat::ParticlePoint point{ x, y, z };
						for (bool skybox : { false, true }) {
							const int expected = FindSegByPosExhaustive({ point }, skybox, start);
							if (world.locate(point, skybox, start) != expected) throw std::runtime_error("Actual source particle segment lookup differs");
							++locations;
						}
						xl_compat::FireParticle actual;
						actual.segment = start;
						actual.position = point;
						CParticle source;
						source.m_nSegment = start;
						source.m_vPos = point;
						nPartSeg[0] = -1;
						wallNorm[0] = nullptr;
						const auto normal = world.collide(actual);
						const bool hit = source.CollideWithWall(0) != 0;
						if (bool(normal) != hit || actual.segment != source.m_nSegment || (normal && (!wallNorm[0] || *normal != wallNorm[0]->point())))
							throw std::runtime_error("Actual source particle side collision differs");
						++collisions;
					}
	}
	std::cout << "Pinned particle world: " << locations << " exhaustive lookups and " << collisions << " side collision/portal comparisons, tolerance boundaries and supplied compiled geometry passed\n";
}
