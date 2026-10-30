// Actual pinned XL heap and route consumers; storage and geometry are adapters
#include "lighting.h"
#include "audio_spatial.h"
#include "runtime/visibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
xl_compat::AudioSpatialOutput audio_geometry_reference_output(int, const xl_compat::FixedLightPoint &, const xl_compat::FixedLightPoint &, const xl_compat::FixedLightPoint &, const xl_compat::FixedLightPoint &, int, int, int, bool, int);
namespace route_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETVAL(x)            return x;
#define RETURN               return;
#define DBG                  0
#define SPARSE_RESET         1
#define FAST_SCAN            1
#define SEGMENT_SIDE_COUNT   6
#define WID_TRANSPARENT_FLAG 1
#define WID_PASSABLE_FLAG    1
#include "xl_reference_audio_constants.h"
void PrintLog(int, const char *, ...) {}
template <class T>
struct CArray {
	std::vector<T> data;
	bool Create(unsigned count, const char * = nullptr)
	{
		data.resize(count);
		return true;
	}
	void Destroy()
	{
		data.clear();
	}
	void Clear(int byte)
	{
		std::memset(data.data(), byte, data.size() * sizeof(T));
	}
	T *Buffer(unsigned index = 0)
	{
		return data.empty() ? nullptr : data.data() + index;
	}
	unsigned Length() const
	{
		return unsigned(data.size());
	}
	T &operator[](unsigned index)
	{
		return data.at(index);
	}
};
template <class T>
struct CStack : CArray<T> {
	unsigned used = 0;
	void Reset()
	{
		used = 0;
	}
	unsigned ToS() const
	{
		return used;
	}
	bool Push(T value)
	{
		if (used >= this->data.size()) throw std::runtime_error("Reference dirty stack overflow");
		this->data[used++] = value;
		return true;
	}
};
using CShortArray = CArray<std::int16_t>;
using CUIntArray = CArray<std::uint32_t>;
#include "xl_reference_audio_heap_class.h"
struct CFixVector {
	static const CFixVector ZERO;
	static fix NormalizedDir(CFixVector &result, const CFixVector &sound, const CFixVector &listener)
	{
		const auto distance = Dist(sound, listener);
		for (unsigned i = 0; i < 3; ++i) result.point[i] = distance ? int((std::int64_t(sound.point[i]) - listener.point[i]) * 65536 / distance) : 0;
		return distance;
	}
	xl_compat::FixedLightPoint point{};
	static fix Dist(const CFixVector &a, const CFixVector &b)
	{
		double sum = 0;
		for (unsigned i = 0; i < 3; ++i) {
			const double d = double(a.point[i]) - b.point[i];
			sum += d * d;
		}
		return fix(std::sqrt(sum));
	}
};
const CFixVector CFixVector::ZERO{};
struct CSegment {
	int ChildIndex(int child) const
	{
		for (unsigned i = 0; i < 6; ++i)
			if (m_children[i] == child) return int(i);
		throw std::runtime_error("Missing reference endpoint edge");
	}
	std::array<int, 6> m_children{ { -1, -1, -1, -1, -1, -1 } };
	int m_childDists[2][6]{};
	std::array<bool, 6> passable{};
	CFixVector center;
	int IsPassable(int side, void *)
	{
		return passable[unsigned(side)] ? 1 : 0;
	}
	CFixVector &Center()
	{
		return center;
	}
};
std::vector<CSegment> segments;
CSegment *segment(int index)
{
	return index >= 0 && unsigned(index) < segments.size() ? &segments[unsigned(index)] : nullptr;
}
#define SEGMENT(i) segment(i)
struct {
	struct {
		std::vector<std::vector<bool>> visible;
		bool SegVis(int a, int b)
		{
			return visible.at(unsigned(a)).at(unsigned(b));
		}
	} segData;
} gameData;
struct CDACSUniDirRouter {
	CDialHeap m_heap;
	int m_nNodes = 0, m_nStartSeg = -1, m_nDestSeg = -1, m_cacheType = -1, m_widFlag = 1;
	CFixVector m_p0, m_p1;
	struct {
		void Add(int, int, int, int) {}
	} m_cache[2];
	int StartSeg() const
	{
		return m_nStartSeg;
	}
	int DestSeg() const
	{
		return m_nDestSeg;
	}
	void SetStartSeg(int segment)
	{
		m_nStartSeg = segment;
	}
	void SetDestSeg(int segment)
	{
		m_nDestSeg = segment;
	}
	int16_t RouteLength(int16_t node)
	{
		return m_heap.RouteLength(node);
	}
	auto Route(unsigned index)
	{
		return m_heap.Route(index);
	}
	int PathLength(const CFixVector &a, int start, const CFixVector &b, int destination, int, int flags, int cache)
	{
		m_p0 = a;
		m_p1 = b;
		m_nStartSeg = start;
		m_nDestSeg = destination;
		m_widFlag = flags;
		m_cacheType = cache;
		return FindPath();
	}
	void SetSize(int count)
	{
		m_nNodes = count;
	}
	bool Create(int32_t);
	fix BuildPath(int16_t);
	fix FindPath();
	fix Distance(int16_t);
};
CDACSUniDirRouter uniDacsRouter[1];
struct CAudio {
	CDACSUniDirRouter m_router[1];
	int m_nListenerSeg = -1;
#define MAX_THREADS 1
#include "xl_reference_audio_update.h"
#undef MAX_THREADS
	bool HaveRouter(int) const
	{
		return true;
	}
	int32_t Distance(CFixVector &, int16_t, CFixVector &, int16_t, fix, int32_t, CFixVector &, int32_t);
};
#include "xl_reference_audio_routes.h"
} // namespace route_reference

void audio_geometry_runtime_controls(const char *path)
{
	using namespace route_reference;
	std::ifstream input(path);
	std::string magic;
	int version = 0;
	unsigned count = 0;
	if (!(input >> magic >> version >> count) || magic != "XL_AUDIO_GEOMETRY" || version != 1 || !count || count > 32767) throw std::runtime_error("Invalid loaded audio geometry header");
	segments.assign(count, {});
	gameData.segData.visible.assign(count, std::vector<bool>(count));
	for (auto &segment : segments) {
		for (auto &value : segment.center.point) input >> value;
		for (unsigned side = 0; side < 6; ++side) input >> segment.m_children[side] >> segment.m_childDists[0][side] >> segment.m_childDists[1][side] >> segment.passable[side];
	}
	for (unsigned a = 0; a < count; ++a)
		for (unsigned b = 0; b < count; ++b) {
			bool visible = false;
			input >> visible;
			gameData.segData.visible[a][b] = visible;
		}
	if (!input) throw std::runtime_error("Truncated loaded audio geometry");
	CAudio source;
	if (!source.m_router[0].Create(int(count))) throw std::runtime_error("Cannot allocate loaded source audio router");
	unsigned cases = 0;
	int listener, sound, volume, range, decay, reverse, hires;
	while (input >> listener) {
		CFixVector a, b, direction;
		xl_compat::FixedLightPoint right, up;
		int volume_actual, pan_actual;
		input >> sound >> volume >> range >> decay >> reverse >> hires;
		for (auto *point : { &a.point, &b.point, &right, &up })
			for (auto &value : *point) input >> value;
		input >> volume_actual >> pan_actual;
		if (!input || listener < 0 || sound < 0 || unsigned(listener) >= count || unsigned(sound) >= count) throw std::runtime_error("Invalid loaded audio geometry case");
		const auto distance = source.Distance(a, int16_t(listener), b, int16_t(sound), range, decay, direction, 0);
		const auto expected = audio_geometry_reference_output(distance, right, up, a.point, b.point, volume, range, decay, reverse != 0, hires);
		if (expected.volume != volume_actual || expected.pan != pan_actual) throw std::runtime_error("Loaded audio geometry differs from source at case " + std::to_string(cases));
		++cases;
	}
	if (!input.eof() || cases != count * 24) throw std::runtime_error("Loaded audio geometry case membership differs");
	std::cout << "PASS: " << cases << " loaded-mine spatial audio outputs match actual source distance, ear angle, volume and pan\n";
}

void audio_route_source_controls()
{
	using namespace route_reference;
	const auto require = [](bool value, const char *message) {if(!value)throw std::runtime_error(message); };
	std::mt19937 random(0x584c524f);
	unsigned graphs = 0, paths = 0;
	for (unsigned count : { 1u, 2u, 3u, 4u, 9u, 31u, 127u })
		for (unsigned pattern = 0; pattern < 12; ++pattern) {
			segments.assign(count, {});
			gameData.segData.visible.assign(count, std::vector<bool>(count, false));
			std::vector<std::array<xl_compat::GeometryRouteEdge, 6>> graph(count);
			for (unsigned i = 0; i < count; ++i) {
				segments[i].center.point = { int(i * 65536u), int((i % 7) * 23456u), int((i % 3) * 54321u) };
				for (unsigned j = 0; j < count; ++j) gameData.segData.visible[i][j] = i == j || (pattern % 3 == 0) || ((pattern % 3 == 2) && (random() % 2));
				for (unsigned side = 0; side < 6; ++side) {
					if ((random() % 4) == 0) continue;
					const int child = int(random() % count);
					const unsigned cost = pattern % 4 == 0 ? 1u : pattern % 4 == 1 ? 65535u
					                                          : pattern % 4 == 2   ? 32768u
					                                                               : 1u + random() % 65535u;
					const bool passable = random() % 5 != 0;
					segments[i].m_children[side] = child;
					segments[i].m_childDists[1][side] = int(cost);
					segments[i].passable[side] = passable;
					if (passable) graph[i][side] = { child, std::uint16_t(cost) };
				}
			}
			CDACSUniDirRouter source;
			require(source.Create(int(count)), "Cannot allocate source audio router");
			for (unsigned start : { 0u, count / 2, count - 1 }) {
				source.m_nStartSeg = int(start);
				source.m_nDestSeg = -1;
				source.FindPath();
				const auto actual = xl_compat::geometry_weighted_routes(graph, int(start));
				for (unsigned destination = 0; destination < count; ++destination) {
					const auto raw = source.m_heap.Cost(int16_t(destination));
					const int cost = raw == 0xffffffffu ? -1 : int(raw & 0x7fffffffu);
					const auto path = xl_compat::geometry_route_path(actual, int(destination));
					require(actual[destination].cost == cost, "Owned audio route costs differ from actual Dial heap");
					const int distance = source.Distance(int16_t(destination));
					if (cost < 0) require(path.empty() && distance == -1, "Unreachable source route was admitted");
					else {
						const int length = source.m_heap.RouteLength(int16_t(destination));
						require(int(path.size()) == length, "Source audio route length differs");
						for (int i = 0; i < length; ++i) require(path[unsigned(i)] == source.m_heap.Route(unsigned(i))->nNode, "Dial tie order changed the source path");
						const auto shortened = xl_compat::geometry_shorten_route(path, [](int a, int b) { return gameData.segData.SegVis(a, b); }, [](int a, int b) { return CFixVector::Dist(segments[unsigned(a)].center, segments[unsigned(b)].center); });
						require(shortened.distance == distance, "Source visibility shortening or endpoint exclusion differs");
					}
					++paths;
				}
				++graphs;
			}
		}
	std::cout << "PASS: " << graphs << " actual source heap/router traversals and " << paths << " weighted paths, tie order, wrap, blocked edges and visibility-shortened interiors\n";
	xl_compat::ClassicRouteGraph native_graph;
	constexpr unsigned node_count = 7;
	segments.assign(node_count, {});
	native_graph.centers.resize(node_count);
	native_graph.edges.resize(node_count);
	std::vector<std::array<xl_compat::GeometryRouteEdge, 6>> native_edges(node_count);
	for (unsigned i = 0; i < node_count; ++i) native_graph.centers[i] = segments[i].center.point = { int(i * 65536u), int((i % 3) * 65536u), 0 };
	const auto connect = [&](unsigned a, unsigned side, unsigned b) {
		const int distance = CFixVector::Dist(segments[a].center, segments[b].center);
		segments[a].m_children[side] = int(b);
		segments[a].m_childDists[0][side] = distance;
		segments[a].m_childDists[1][side] = distance / 1024;
		segments[a].passable[side] = true;
		native_graph.edges[a][side] = { int(b), distance, std::uint16_t(distance / 1024), true };
		native_edges[a][side] = { int(b), std::uint16_t(distance / 1024) };
	};
	for (unsigned i = 0; i + 1 < node_count; ++i) {
		connect(i, 0, i + 1);
		connect(i + 1, 1, i);
	}
	connect(0, 2, 3);
	connect(3, 2, 0);
	unsigned distance_controls = 0;
	for (unsigned visibility = 0; visibility < 3; ++visibility) {
		gameData.segData.visible.assign(node_count, std::vector<bool>(node_count, false));
		for (unsigned a = 0; a < node_count; ++a)
			for (unsigned b = 0; b < node_count; ++b)
				gameData.segData.visible[a][b] = a == b || visibility == 1 || (visibility == 2 && a % 2 == b % 2);
		for (unsigned listener = 0; listener < node_count; ++listener) {
			CAudio source;
			source.m_router[0].Create(node_count);
			const auto routes = xl_compat::geometry_weighted_routes(native_edges, int(listener));
			for (unsigned sound = 0; sound < node_count; ++sound)
				for (int range : { 1, 2, 65536, 5 * 65536, 256 * 65536 })
					for (int decay : { 0, 1, 3 })
						for (int offset : { -32768, 0, 32768 }) {
							auto a = segments[listener].center, b = segments[sound].center;
							a.point[0] += offset;
							b.point[1] -= offset;
							CFixVector direction;
							const int expected = source.Distance(a, int16_t(listener), b, int16_t(sound), range, decay, direction, 0);
							const int actual = xl_compat::audio_geometry_distance(native_graph, gameData.segData.visible, routes, int(listener), int(sound), a.point, b.point, range, decay);
							require(actual == expected, "Source audio distance range, visibility correction or endpoint adjustment differs");
							++distance_controls;
						}
		}
	}
	std::cout << "PASS: " << distance_controls << " actual source audio-distance comparisons, same-segment boundaries, route interiors and endpoint corrections\n";
	gameData.segData.visible.assign(node_count, std::vector<bool>(node_count, false));
	CAudio source;
	source.m_router[0].Create(node_count);
	auto a = segments[0].center, b = segments[6].center;
	CFixVector direction;
	const auto distance = [&] { return source.Distance(a, 0, b, 6, 256 * 65536, 0, direction, 0); };
	segments[3].passable[0] = segments[4].passable[1] = false;
	require(distance() < 0, "Closed source audio route was reachable");
	segments[3].passable[0] = segments[4].passable[1] = true;
	require(distance() < 0, "Source unexpectedly refreshed an unchanged listener's cached route");
	source.Update();
	require(source.m_router[0].StartSeg() == -1 && source.m_router[0].DestSeg() == -1 && source.m_nListenerSeg == 0, "Source update changed listener identity or retained router endpoints");
	const auto opened = distance();
	require(opened > 0, "Source update failed to observe an opened route");
	segments[3].passable[0] = segments[4].passable[1] = false;
	require(distance() == opened, "Source unexpectedly discarded a cached open route");
	source.Update();
	require(distance() < 0, "Source update failed to observe a closed route");
	std::cout << "PASS: stationary-listener source route invalidation, stale closed/open routes and preserved listener identity\n";
}
