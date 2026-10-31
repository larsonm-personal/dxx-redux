// Actual source visibility bodies; storage, scene flags and exhaustive lookup are supplied
#include "particle_visibility.h"
#include "lightning_visibility.h"
#include <iostream>
#include <cstring>
namespace particle_visibility_reference
{
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v{};
	void Set(int x, int y, int z)
	{
		v.coord = { x, y, z };
	}
	static const CFixVector Avg(const CFixVector &, const CFixVector &);
	static int Dist(const CFixVector &, const CFixVector &)
	{
		throw std::runtime_error("Scratch visible flags unexpectedly reached a distance query");
	}
};
struct CFloatVector {
	struct {
		struct {
			float x, y, z;
		} coord;
	} v{};
	CFloatVector operator-(const CFloatVector &b) const
	{
		return { { { v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y, v.coord.z - b.v.coord.z } } };
	}
	CFloatVector &Assign(const CFixVector &);
	const float SqrMag() const;
	const float Mag() const;
	static const float Dist(const CFloatVector &, const CFloatVector &);
};
template <class T>
struct Array {
	std::vector<T> values;
	bool Create(int size, const char *)
	{
		values.resize(std::size_t(size));
		return true;
	}
	void Clear(int filler)
	{
		std::memset(values.data(), filler, values.size() * sizeof(T));
	}
	T *Buffer()
	{
		return values.data();
	}
	T &operator[](int index)
	{
		return values.at(std::size_t(index));
	}
};
struct tSegZRef {
	int z, nSegment;
};
struct CVisibilityData {
	int nSegments;
	uint8_t nVisible, nVisited, nProcessed;
	Array<int16_t> segments;
	Array<uint8_t> bVisited, bVisible, bProcessed;
	Array<int> points, nDepth, portals, position;
	Array<tSegZRef> zRef[2];
	CVisibilityData();
	bool Create(int32_t);
	uint8_t BumpVisitedFlag();
	uint8_t BumpProcessedFlag();
	int32_t SegmentMayBeVisible(int16_t, int32_t, int32_t);
	void InitZRef(int32_t, int32_t, int32_t);
};
struct CSegment {
	CFixVector center;
	int radius = 0, m_function = 0;
	int16_t m_children[6]{ -1, -1, -1, -1, -1, -1 };
	int passable[6]{};
	const CFixVector &Center() const
	{
		return center;
	}
	int MaxRad() const
	{
		return radius;
	}
	int IsPassable(int side, void *) const
	{
		return passable[side];
	}
};
std::vector<CSegment> source_segments;
struct {
	struct {
		struct {
			CVisibilityData visibility[3];
			std::vector<bool> flags;
			std::array<int, 1> bObjectRendered{};
			struct {
				CFixVector vPos;
			} viewer;
			bool Visible(int index) const
			{
				return index >= 0 && std::size_t(index) < flags.size() && flags[index];
			}
		} mine;
	} renderData;
	struct {
		int nSegments = 8;
	} segData;
} gameData;
struct {
	struct {
		int nFrameFlipFlop = 1;
	} render;
} gameStates;
struct CLightning {
	int m_nObject = -1, m_nSegment = -1, m_nLength = 0;
	CFixVector m_vPos, m_vEnd;
	int32_t MayBeVisible(int32_t);
};
std::array<int, 2> lightning_locations{};
std::vector<xl_compat::ParticlePoint> lightning_queries;
int FindSegByPos(const CFixVector &point, int start, int exhaustive, int skybox)
{
	if (start != -1 || exhaustive != 1 || skybox != 0 || lightning_queries.size() >= 2) throw std::runtime_error("Lightning source lookup policy changed");
	const int result = lightning_locations[lightning_queries.size()];
	lightning_queries.push_back({ point.v.coord.x, point.v.coord.y, point.v.coord.z });
	return result;
}
struct {
	int zMin[1]{}, zMax[1]{};
} tiRender;
int location = -1, locate_calls = 0;
int FindSegByPosExhaustive(const CFixVector &, bool, int)
{
	++locate_calls;
	return location;
}
struct CParticle {
	int m_nSegment = -1;
	CFixVector m_vPos;
	bool m_bSkybox = false;
	bool IsVisible(int32_t);
};
struct CParticleEmitter {
	int m_nSegment = -1;
	int32_t MayBeVisible(int32_t);
};
#define USE_OPENMP           0
#define TRANSP_DEPTH_HASH    1
#define LEVEL_SEGMENTS       8
#define LEVEL_VERTICES       64
#define SEGMENT_SIDE_COUNT   6
#define SEGMENT_FUNC_SKYBOX  10
#define WID_TRANSPARENT_FLAG 4
#define SEGMENT(i)           (&source_segments.at(std::size_t(i)))
#define X2F(a)               (float(a) / 65536.0f)
#define F2X(a)               (int32_t((a) * 65536.0f))
#define I2X(a)               ((a) * 65536)
#include "xl_reference_particle_visibility.h"
} // namespace particle_visibility_reference

void particle_visibility_source_controls()
{
	using namespace particle_visibility_reference;
	auto &mine = gameData.renderData.mine;
	source_segments.resize(LEVEL_SEGMENTS);
	std::vector<xl_compat::ParticleSegment> segments(LEVEL_SEGMENTS);
	for (int i = 0; i < LEVEL_SEGMENTS; ++i) {
		segments[i].center = { i * 771331 - 1900000, i * 91133 - 109811, i * -291391 + 711999 };
		segments[i].outer_radius = 131111 + i * 901;
		source_segments[i].center.v.coord = { segments[i].center[0], segments[i].center[1], segments[i].center[2] };
		source_segments[i].radius = segments[i].outer_radius;
		// A cycle with both transparent and blocked links exercises scratch BFS
		for (int side = 0; side < 2; ++side) {
			const int child = (i + 1 + side) % LEVEL_SEGMENTS;
			source_segments[i].m_children[side] = int16_t(child);
			source_segments[i].passable[side] = side ? 0 : WID_TRANSPARENT_FLAG;
			segments[i].sides[side].child = child;
		}
	}
	if (!mine.visibility[2].Create(1)) throw std::runtime_error("Source scratch visibility allocation failed");
	unsigned emitters = 0, particles = 0, depths = 0, bolts = 0;
	for (unsigned mask = 0; mask < 256; ++mask) {
		mine.flags.assign(LEVEL_SEGMENTS, false);
		std::vector<int> visible;
		for (int i = 0; i < LEVEL_SEGMENTS; ++i) {
			mine.flags[i] = (mask & (1u << i)) != 0;
			if (mine.flags[i]) visible.push_back(i);
		}
		for (int segment = -1; segment < LEVEL_SEGMENTS; ++segment)
			for (bool skybox : { false, true }) {
				if (segment >= 0) source_segments[segment].m_function = skybox ? SEGMENT_FUNC_SKYBOX : 0;
				CParticleEmitter emitter{ segment };
				if (bool(emitter.MayBeVisible(0)) != xl_compat::particle_emitter_visible(segment, skybox, mine.flags)) throw std::runtime_error("Emitter visibility differs from initialized source scratch state");
				++emitters;
			}
		for (int segment : { -1, 0, 3, 7, LEVEL_SEGMENTS })
			for (int found : { -1, 0, 4, 7 }) {
				location = found;
				locate_calls = 0;
				CParticle source;
				source.m_nSegment = segment;
				const bool expected = source.IsVisible(0);
				int calls = 0;
				xl_compat::FireParticle particle{};
				particle.segment = segment;
				const bool actual = xl_compat::particle_visible(particle, segments, mine.flags, [&](const xl_compat::ParticlePoint &, bool, int) { ++calls; return found; });
				if (actual != expected || particle.segment != source.m_nSegment || calls != locate_calls) throw std::runtime_error("Particle visibility, relocation or lookup cadence differs from source");
				++particles;
			}
		for (int owner : { -1, 0, 1 })
			for (int segment : { -1, 0, 3, 7 })
				for (int length : { 0, 20 * 65536, 50 * 65536 })
					for (int end : { -1, 0, 4, 7 })
						for (int middle : { -1, 0, 4, 7 }) {
							CLightning source;
							source.m_nObject = owner < 0 ? -1 : 0;
							source.m_nSegment = segment;
							source.m_nLength = length;
							source.m_vPos.Set(-100001, 199999, -399999);
							source.m_vEnd.Set(200000, -400000, 300000);
							mine.bObjectRendered[0] = owner == 1 ? gameStates.render.nFrameFlipFlop : 0;
							lightning_locations = { end, middle };
							lightning_queries.clear();
							const bool expected = source.MayBeVisible(0) != 0;
							std::vector<xl_compat::ParticlePoint> queries;
							const bool actual = xl_compat::lightning_visible(owner == 1, segment, { -100001, 199999, -399999 }, { 200000, -400000, 300000 }, mine.flags, [&](const xl_compat::ParticlePoint &point) {
								const int result = lightning_locations.at(queries.size());
								queries.push_back(point);
								return result;
							});
							if (actual != expected || queries != lightning_queries) throw std::runtime_error("Lightning visibility or ordered endpoint/midpoint queries differ from source");
							++bolts;
						}
		if (!visible.empty()) {
			auto &state = mine.visibility[2];
			for (unsigned i = 0; i < visible.size(); ++i) state.segments[int(i)] = int16_t(visible[i]);
			const xl_compat::ParticlePoint eye{ int(mask) * 41191 - 1000000, -111111, int(mask) * -73711 + 2000000 };
			mine.viewer.vPos.v.coord = { eye[0], eye[1], eye[2] };
			state.InitZRef(0, int(visible.size()), 0);
			if (xl_compat::particle_depth_limit(segments, visible, eye) != tiRender.zMax[0]) throw std::runtime_error("Particle visible-scene depth bound differs from source");
			++depths;
		}
	}
	if (xl_compat::particle_depth_limit(segments, {}, {}) != 0) throw std::runtime_error("Empty particle scene has a depth range");
	std::cout << "Pinned particle visibility: " << emitters << " emitter and " << particles << " particle selections, relocation/call counts and " << depths << " source depth bounds passed\n";
	std::cout << "Pinned lightning visibility: " << bolts << " owner/start/end/midpoint selections and ordered lookup positions passed\n";
}
