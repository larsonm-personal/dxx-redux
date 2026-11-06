#include "../model_geometry.h"
#include "../model_hitbox.h"
#include "../model_hitbox_query.h"
#include "../camera_unstick.h"
#include "../camera_hit_state.h"
#include "../camera_hit_render.h"
#include "../shield_sphere.h"
#include "../shield_pulse.h"
#include "../player_shield.h"
#include "../addon_animation.h"
#include <ctime>
#include "../model_render_geometry.h"
#include "../model_sphere_query.h"
#include "../camera_sphere_unstick.h"
#include "../inventory.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include "reference_collision_profile.h"

using fix = int32_t;
struct {
	struct {
		int nMathFormat = 2;
		struct {
			int nQuality = 2;
		} textures;
	} render;
} options;
auto *gameOpts = &options;
struct tQuadInt {
	int low, high;
};
static void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected legacy hitbox math");
}
static fix QuadSqrt(int, int)
{
	throw std::runtime_error("Unexpected legacy hitbox math");
}
struct CFloatVector;
struct CFixVector {
	union {
		struct {
			int32_t x, y, z;
		} coord;
		int32_t vec[3];
	} v;
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
	static const CFixVector Create(fix x, fix y, fix z)
	{
		return { { { x, y, z } } };
	}
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	const CFixVector operator-(const CFixVector &) const;
	static const CFixVector Normal(const CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Perp(const CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Cross(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	static const fix Dist(const CFixVector &, const CFixVector &);
	static const CFixVector Avg(const CFixVector &, const CFixVector &);
	CFixVector &Assign(const CFloatVector &);
	const CFixVector &operator+=(const CFixVector &);
	const CFixVector &operator/=(fix);
	void SetZero()
	{
		Set(0, 0, 0);
	}
	fix Mag() const;
	void Check();
	bool IsZero() const;
	const CFixVector &operator*=(fix);
	const CFixVector operator*(fix) const;
	bool operator!=(const CFixVector &other) const
	{
		return v.coord.x != other.v.coord.x || v.coord.y != other.v.coord.y || v.coord.z != other.v.coord.z;
	}
};
struct CFloatVector {
	union {
		struct {
			float x, y, z, w;
		} coord;
		float vec[4];
	} v;
	CFloatVector &Assign(const CFixVector &);
	static const float Dot(const CFloatVector &, const CFloatVector &);
	const CFloatVector &operator*=(float);
	const CFloatVector &operator/=(float);
	const CFloatVector &operator+=(const CFloatVector &);
	const CFloatVector &operator-=(const CFloatVector &);
	const bool operator==(const CFloatVector &);
	const CFloatVector operator+(const CFloatVector &) const;
	const CFloatVector operator-(const CFloatVector &) const;
	const CFloatVector operator*(float) const;
	void Set(float, float, float, float = 1);
	const float SqrMag() const;
	const float Mag() const;
	static const float Normalize(CFloatVector &);
	static const float Dist(const CFloatVector &, const CFloatVector &);
	void SetZero()
	{
		Set(0, 0, 0, 0);
	}
};
struct CFixVector2 {
	fix m_x, m_y;
	CFixVector2(fix x, fix y) : m_x(x), m_y(y) {}
};
struct tQuad {
	CFixVector n[2];
	CFixVector v[4];
};
struct tBox {
	CFixVector vertices[8];
	tQuad faces[6];
};
struct tHitbox {
	CFixVector vMin, vMax, vOffset, vSize;
	tBox box;
};
constexpr int MAX_HITBOXES = 64;
struct CModelHitboxList {
	tHitbox hitboxes[MAX_HITBOXES + 1];
	int nHitboxes, nFrame;
};
struct {
	struct {
		CModelHitboxList hitboxes[2];
		CFixVector offsets[2];
	} modelData;
	struct {
		int nFrameCount;
	} objData;
} gameData;
struct CFixMatrix {
	union {
		struct {
			CFixVector r, u, f;
		} dir;
		fix vec[9];
	} m;
	CFixVector operator*(const CFixVector &);
	static CFixMatrix &Transpose(CFixMatrix &);
	const fix Det();
	const CFixMatrix Inverse();
};
struct CPhysSimData;
struct CFile {
	std::vector<int32_t> words;
	std::size_t cursor = 0;
	void WriteFix(int32_t value)
	{
		words.push_back(value);
	}
	int32_t ReadFix()
	{
		return words.at(cursor++);
	}
};
static struct {
	int Version() const
	{
		return 46;
	}
} saveGameManager;
struct CObject {
	int model = 0;
	bool polygon = true;
	unsigned registered_hits = 0;
	bool apply_hit_state = false;
	CFixVector registered_point{};
	int16_t registered_model = -1;
	fix model_radius = 8 * 65536;
	struct {
		fix nHits[3]{};
		bool bCritical = false;
		int32_t nCritical = 0, tCritical = 0, tShield = 0, tRepaired = 0;
	} m_damage;
	struct {
		CFixVector v[3]{};
		int32_t t[3]{};
		int i = 0;
	} m_hitInfo;
	struct {
		struct {
			CFixVector vPos;
			CFixMatrix mOrient;
		} position;
		fix xSize;
		int nType = 4;
		int renderType = 1;
	} info;
	struct {
		struct {
			struct {
				int nType;
			} parent;
		} laserInfo;
	} cType{};
	CFixMatrix view;
	const auto &HitInfo() const
	{
		return m_hitInfo;
	}
	fix Size() const
	{
		return info.xSize;
	}
	int Type() const
	{
		return info.nType;
	}
	int ModelId() const
	{
		return model;
	}
	CFixMatrix *View(int)
	{
		return &view;
	}
	void UnstickFromObject(CPhysSimData &, CFixVector &);
	void SaveHitStateReference(CFile &);
	void LoadHitStateReference(CFile &);
	bool IsStatic() const
	{
		return false;
	}
	fix ModelRadius(int) const
	{
		return model_radius;
	}
	CFixVector RegisterHit(CFixVector hit, int16_t model_id)
	{
		++registered_hits;
		registered_point = hit;
		registered_model = model_id;
		return apply_hit_state ? RegisterHitSource(hit, model_id) : hit;
	}
	CFixVector RegisterHitSource(CFixVector, int16_t);
	float Damage() const
	{
		throw std::runtime_error("Advanced damage is outside default profile");
	}
	float DamageRate() const
	{
		throw std::runtime_error("Advanced damage is outside default profile");
	}
};
struct CHitData {
	int nObject;
	CFixVector vPoint{}, vNormal{};
};
struct CPhysSimData {
	CHitData hitResult;
	CFixVector vOffset, vOldPos, vNewPos;
};
static CObject unstick_target;
static std::function<bool(const CFixVector &, const CFixVector &)> unstick_query;
static int CheckVectorObjectCollision(CHitData &, CFixVector *start, CFixVector *end, fix radius, CObject *stationary, CObject *moving, int model, bool visibility)
{
	if (moving != &unstick_target || start != &stationary->info.position.vPos || radius != stationary->info.xSize || model != -1 || visibility)
		throw std::runtime_error("Source unstick reverse-query contract changed");
	return unstick_query(*start, *end);
}
#define OBJECT(index) (&unstick_target)
#define ENTER(a, b)
#define RETURN        return;
#define RETVAL(value) return value;
struct tRobotInfo {
	int attackType;
};
#define ROBOTINFO(actor) static_cast<tRobotInfo *>(nullptr)
static constexpr bool IsCoopGame = false;
static constexpr int OBJ_PLAYER = 4, OBJ_WEAPON = 5, OBJ_POWERUP = 7;
static constexpr int OBJ_ROBOT = 2, OBJ_REACTOR = 9;
struct {
	struct {
		bool bNostalgia = false;
		int32_t nSDLTicks[2]{};
	} app;
} gameStates;
static int shield_effect_time = 2000;
#define SHIELD_EFFECT_TIME shield_effect_time
#define EGI_FLAG(...)      0
static int RandShort()
{
	throw std::runtime_error("Default hit registration consumed RNG");
}
static int UseHitbox(CObject *actor)
{
	return actor->polygon;
}
static int UseSphere(CObject *)
{
	return 0;
}
static constexpr int RT_POLYOBJ = 1;
static fix CheckVectorSphereCollision(CFixVector &, CFixVector *, CFixVector *, CFixVector *, fix);
static int collision_model = 1;
static int CollisionModel(int requested = -1)
{
	return requested < 0 ? collision_model : requested;
}
#define DBG         0
#define OBJPOS(obj) (&(obj)->info.position)
class CPolyModel
{
  public:
	struct {
		int nId = 0;
	} m_info;
	std::vector<uint8_t> data;
	void *Buffer()
	{
		return data.data();
	}
	fix Size();
};
enum { OP_EOF,
	   OP_DEFPOINTS,
	   OP_FLATPOLY,
	   OP_TMAPPOLY,
	   OP_SORTNORM,
	   OP_RODBM,
	   OP_SUBCALL,
	   OP_DEFP_START,
	   OP_GLOW };
static int WORDVAL(const uint8_t *p)
{
	return p[0] | unsigned(p[1]) << 8;
}
#define VECPTR(p) reinterpret_cast<CFixVector *>(p)
static void G3CheckAndSwap(void *) {}
static bool facing;
static bool G3CheckNormalFacing(const CFixVector &, const CFixVector &)
{
	return facing;
}
static int G3ModelMinMax(int, tHitbox *)
{
	return 0;
}
void ComputeHitbox(int, int);
static void Error(const char *message)
{
	throw std::runtime_error(message);
}
struct RenderCount {
	int32_t CountPOFModelItems(void *, uint16_t *, uint16_t *, uint16_t *, uint16_t *);
};
using CObjHitInfo = decltype(CObject{}.m_hitInfo);
static constexpr double PI = 3.141592653589793240;
struct {
	bool hardware = true;
	bool UseTransform() const
	{
		return hardware;
	}
} ogl;
struct {
	// Software-transform identity adapter; that branch's vectors remain unqualified
	void Transform(CFloatVector &out, const CFloatVector &in) const
	{
		out = in;
	}
} transformation;
struct SourceHitEnvelope {
	int active;
	std::array<float, 3> inverse_radius;
	std::array<std::array<float, 4>, 3> vectors;
};
template <typename T>
static void Swap(T &a, T &b)
{
	std::swap(a, b);
}
#include "xl_reference_hxm_geometry.h"
#include "xl_reference_shield_sphere.h"
#undef VECPTR
#include "reference_pof_render.h"

static void require(bool condition, const std::string &message)
{
	if (!condition) throw std::runtime_error(message);
}
#include "reference_shield_pulse.h"
#include "reference_player_shield.h"
#include "reference_addon_animation.h"
static void compare_sphere()
{
	using Point = xl_compat::FixedLightPoint;
	const auto vector = [](const Point &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	const auto point = [](const CFixVector &p) { return Point{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
	unsigned cases = 0, hits = 0, zero = 0, registrations = 0;
	for (Point center : { Point{}, Point{ 12345, -23456, 34567 }, Point{ 79123456, -98234567, 38345678 } })
		for (Point direction : { Point{}, Point{ 65536, 0, 0 }, Point{ 0, -65536, 0 }, Point{ 0, 0, 65536 }, Point{ 131071, -65537, 32769 } })
			for (int offset : { -8, -2, -1, 0, 1, 2, 8 })
				for (int sweep : { -16, -1, 0, 1, 16 })
					for (int radius : { -1, 0, 1, 32768, 65536, 65537, 297534 })
						for (int transverse : { 0, 1, 65536, 65537 }) {
							Point start = center, end, actual{ 1, 2, 3 };
							for (unsigned axis = 0; axis < 3; ++axis) start[axis] += direction[axis] * offset;
							start[1] += transverse;
							end = start;
							for (unsigned axis = 0; axis < 3; ++axis) end[axis] += direction[axis] * sweep;
							auto p0 = vector(start), p1 = vector(end), sphere = vector(center), expected_hit = vector(actual);
							const int expected = CheckVectorSphereCollision(expected_hit, &p0, &p1, &sphere, radius);
							const int result = xl_compat::model_sphere_query(actual, start, end, center, radius);
							if (result != expected || actual != point(expected_hit)) {
								std::cerr << "Sphere mismatch case " << cases << " radius " << radius << " result " << result << " expected " << expected;
								for (unsigned axis = 0; axis < 3; ++axis) std::cerr << " axis" << axis << " start=" << start[axis] << " end=" << end[axis] << " center=" << center[axis] << " hit=" << actual[axis] << " expected=" << point(expected_hit)[axis];
								std::cerr << '\n';
							}
							require(result == expected && actual == point(expected_hit), "Floating sphere query differs from extracted source");
							require(point(p0) == start && point(p1) == end, "Source fixed sphere wrapper changed input endpoints");
							CObject stationary{}, moving{};
							stationary.polygon = moving.polygon = false;
							stationary.info.nType = 16;
							stationary.info.position.vPos = sphere;
							stationary.info.xSize = radius < 0 ? 0 : radius;
							moving.info.nType = 5;
							CHitData data{};
							const int outer = ReferenceVectorObjectCollision(data, &p0, &p1, radius < 0 ? radius : 0, &stationary, &moving, -1, false);
							require(outer == (expected < 0 ? 0 : expected) && stationary.registered_hits == unsigned(expected >= 0), "Default sphere outer query changed return or registration reachability");
							if (expected >= 0) require(point(data.vPoint) == actual && point(data.vNormal) == Point{} && stationary.registered_model == 0, "Default sphere outer query changed contact point, normal or model");
							++cases;
							hits += result >= 0;
							zero += result == 0;
							registrations += stationary.registered_hits;
						}
	require(hits && zero, "Sphere controls missed contacts or zero-distance behavior");
	std::cout << "Source float sphere queries: " << cases << " passed, " << hits << " contacts, " << zero << " zero results, " << registrations << " outer registrations\n";
}
static void compare_unstick()
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	const auto vector = [](const Point &v) { return CFixVector::Create(v[0], v[1], v[2]); };
	unsigned cases = 0, queries = 0, longest = 0;
	for (bool stopped : { false, true })
		for (bool moved : { false, true })
			for (int extent : { 1, -1, 65537, -65537, 4194305 })
				for (int size : { 1, 65537, 400001 })
					for (unsigned pattern : { 0u, 0xffffffffu, 0xaaaaaaaau, 0x55555555u, 0xffff00ffu })
						for (int target : { -123456, 0, 765432 }) {
							const Point origin{ 317, -912, 65537 }, endpoint = moved ? Point{ 65536, 131072, -65536 } : origin;
							const Point offset{ extent, extent / 3, -extent / 7 }, velocity = stopped ? Point{} : Point{ 1, 2, -3 };
							CObject actor{};
							actor.info.position.vPos = vector(origin);
							actor.info.xSize = size;
							unstick_target.info.position.vPos = target ? CFixVector::Create(target, -target / 2, target / 3) : vector(origin);
							CPhysSimData source{ { 0 }, vector(offset), vector(origin), vector(endpoint) };
							auto source_velocity = vector(velocity);
							std::vector<std::pair<Point, Point>> trace;
							unstick_query = [&](const CFixVector &a, const CFixVector &b) {
								const auto index = trace.size();
								trace.emplace_back(point(a), point(b));
								return (pattern >> (index % 32)) & 1u;
							};
							actor.UnstickFromObject(source, source_velocity);
							xl_compat::CameraUnstickState state{ origin, endpoint, offset };
							auto position = origin;
							unsigned index = 0;
							xl_compat::camera_unstick(state, position, point(unstick_target.info.position.vPos), velocity, size, [&](const Point &a, const Point &b) {
								require(index < trace.size() && trace[index] == std::make_pair(a, b), "Unstick query sequence differs from source");
								return (pattern >> (index++ % 32)) & 1u;
							});
							require(index == trace.size() && position == point(actor.info.position.vPos) && state.new_position == point(source.vNewPos) && state.offset == point(source.vOffset), "Unstick final state differs from source");
							++cases;
							queries += index;
							longest = std::max(longest, index);
						}
	require(longest > 8, "Unstick comparisons did not exercise extended overlap search");
	unstick_query = {};
	std::cout << "Source camera unstick: " << cases << " cases, " << queries << " queries, longest " << longest << '\n';
}
static void compare_hit_state()
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	const auto vector = [](const Point &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	unsigned cases = 0;
	for (int duration : { 500, 2000 })
		for (bool nostalgia : { false, true })
			for (int type : { 2, 4, 5, 9, 16 })
				for (int size : { 1, 65537, 400001 })
					for (int critical : { 0, 1000, 3000 })
						for (unsigned slot = 0; slot < 3; ++slot) {
							CObject source{};
							source.info.nType = type;
							source.info.xSize = size;
							const Point position{ 317, -912, 65537 };
							source.info.position.vPos = vector(position);
							source.m_damage.tCritical = critical;
							source.m_hitInfo.i = int(slot);
							xl_compat::CameraHitState native;
							native.critical_time = critical;
							native.next = slot;
							shield_effect_time = duration;
							gameStates.app.bNostalgia = nostalgia;
							for (int step = 0; step < 24; ++step) {
								const int ticks = step < 6 ? critical + duration / 4 + step - 3 : step * 751;
								gameStates.app.nSDLTicks[0] = ticks;
								Point hit = position;
								if (step % 7) {
									hit[step % 3] += step % 2 ? 65537 : -65537;
									hit[(step + 1) % 3] += step % 4 ? 1 : 17;
								}
								const auto expected = source.RegisterHitSource(vector(hit), -1);
								const auto actual = native.register_hit(hit, position, size, type, ticks, duration, nostalgia);
								require(actual == point(expected) && native.repaired == source.m_damage.tRepaired && native.shield_time == source.m_damage.tShield && native.critical_time == source.m_damage.tCritical && native.next == unsigned(source.m_hitInfo.i), "Default hit registration state differs from source");
								for (unsigned i = 0; i < 3; ++i) require(native.points[i] == point(source.m_hitInfo.v[i]) && native.times[i] == source.m_hitInfo.t[i], "Default shield hit history differs from source");
								++cases;
							}
						}
	gameStates.app.bNostalgia = false;
	std::cout << "Source default hit registration: " << cases << " state transitions\n";
}
static void compare_shield_sphere()
{
	unsigned vertices = 0, quality_cases = 0;
	for (int texture = 0; texture <= 3; ++texture)
		for (int cached : { 0, 2, 3, 4 }) {
			gameOpts->render.textures.nQuality = texture;
			CTesselatedSphere source;
			source.m_nQuality = cached;
			require(source.Quality() == xl_compat::shield_sphere_quality(texture, cached), "Shield sphere release quality differs from source");
			++quality_cases;
		}
	for (int quality = 2; quality <= 4; ++quality) {
		CQuadSphere source;
		source.m_nQuality = quality;
		const auto actual = xl_compat::shield_sphere_mesh(quality);
		require(source.Create() > 0 && actual.size() == std::size_t(source.m_nVertices), "Shield sphere source vertex count differs");
		for (unsigned i = 0; i < actual.size(); ++i) {
			const auto &expected = source.m_worldVerts[i];
			for (unsigned axis = 0; axis < 3; ++axis) require(actual[i].position[axis] == expected.m_v.v.vec[axis], "Shield sphere vertex differs from source subdivision/normalization");
			require(actual[i].uv == std::array<float, 2>{ expected.m_tc.v.u, expected.m_tc.v.v }, "Shield sphere UV or triangle order differs from source");
			++vertices;
		}
	}
	for (int quality : { -1, 0, 1, 5, 100 }) {
		bool rejected = false;
		try {
			xl_compat::shield_sphere_mesh(quality);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Shield sphere accepted unsupported tessellation quality");
	}
	gameOpts->render.textures.nQuality = 2;
	std::cout << "Source shield quad sphere: " << vertices << " exact ordered vertices/UVs across qualities2-4, " << quality_cases << " release quality cases passed\n";
}
static void compare_hit_vectors()
{
	using Matrix = xl_compat::HitRenderMatrix;
	using Point = xl_compat::FixedLightPoint;
	const std::array<Matrix, 7> matrices{ { { { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } },
		                                    { { { 0, 65536, 0 }, { 0, 0, 65536 }, { 65536, 0, 0 } } },
		                                    { { { -65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, -65536 } } },
		                                    { { { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } },
		                                    { { { 32768, 0, 0 }, { 0, 131072, 0 }, { 0, 0, 65536 } } },
		                                    {},
		                                    { { { 65536, 0, 0 }, { 65536, 0, 0 }, { 65536, 0, 0 } } } } };
	const std::array<Point, 7> points{ { {}, { 1, 0, 0 }, { -1, 0, 0 }, { 65536, -32768, 16384 }, { 297534, 0, 0 }, { 45678, -1234, 98765 }, { -400001, 200003, -300007 } } };
	unsigned cases = 0, active = 0;
	for (const auto &matrix : matrices) {
		CObject source{};
		source.info.xSize = 297534;
		for (unsigned i = 0; i < 9; ++i) source.info.position.mOrient.m.vec[i] = matrix[i / 3][i % 3];
		auto expected_matrix = source.info.position.mOrient;
		expected_matrix.Transpose(expected_matrix);
		expected_matrix = expected_matrix.Inverse();
		const auto actual_matrix = xl_compat::camera_hit_inverse_transpose(matrix);
		for (unsigned i = 0; i < 9; ++i) require(actual_matrix[i / 3][i % 3] == expected_matrix.m.vec[i], "Hit shader inverse transpose differs from source fixed arithmetic");
		for (unsigned point = 0; point < points.size(); ++point)
			for (int duration : { 500, 2000 })
				for (int age : { -1, 0, 125, 499, 500, 1999, 2000, 2001 }) {
					xl_compat::CameraHitState state;
					state.times = { 10000 - age, 10000 - age / 2, 8000 };
					for (unsigned i = 0; i < 3; ++i) {
						state.points[i] = points[(point + i) % points.size()];
						const auto &v = state.points[i];
						source.m_hitInfo.v[i] = CFixVector::Create(v[0], v[1], v[2]);
						source.m_hitInfo.t[i] = state.times[i];
					}
					gameStates.app.nSDLTicks[0] = 10000;
					shield_effect_time = duration;
					ogl.hardware = true;
					const auto expected = ReferenceHitEnvelope(&source);
					const auto actual = xl_compat::camera_hit_hardware(state, source.info.xSize, 10000, duration, matrix);
					require(actual.envelope.active == unsigned(expected.active) && actual.envelope.inverse_radius == expected.inverse_radius && actual.vectors == expected.vectors,
					        "Hit shader hardware vectors differ from source transform and float normalization");
					active += actual.envelope.active;
					++cases;
				}
	}
	for (int variant = 0; variant < 2; ++variant) {
		Matrix matrix = matrices[0];
		xl_compat::CameraHitState state;
		if (!variant) matrix[0] = matrix[1] = matrix[2] = { INT32_MAX, INT32_MAX, INT32_MAX };
		else {
			matrix[2][2] = 32768;
			state.points[0][2] = INT32_MAX;
		}
		bool rejected = false;
		try {
			xl_compat::camera_hit_hardware(state, 65536, 1, 2000, matrix);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Hit shader fixed overflow was accepted");
	}
	std::cout << "Source hit shader hardware vectors: " << cases << " cases, " << active << " active vectors and seven exact inverse transposes passed\n";
}
static void compare_hit_render()
{
	unsigned cases = 0, active = 0, future = 0;
	for (int size : { 1, 32768, 65536, 108769, 297534, 4000001, INT32_MAX })
		for (int duration : { 500, 2000 })
			for (bool hardware : { false, true })
				for (int a : { -1, 0, 1, 125, 499, 500, 1999, 2000, 10000 })
					for (int b : { -3000, 0, 499, 2000 }) {
						CObject source{};
						source.info.xSize = size;
						gameStates.app.nSDLTicks[0] = 10000;
						shield_effect_time = duration;
						ogl.hardware = hardware;
						xl_compat::CameraHitState state;
						state.times = { 10000 - a, 10000 - b, 0 };
						for (unsigned i = 0; i < 3; ++i) source.m_hitInfo.t[i] = state.times[i];
						const auto expected = ReferenceHitEnvelope(&source);
						const auto actual = xl_compat::camera_hit_envelope(state, size, 10000, duration, hardware);
						require(actual.needed == bool(NeedEffect(&source)) && actual.active == unsigned(expected.active) && actual.inverse_radius == expected.inverse_radius,
						        "Hit shader scalar envelope differs from pinned source");
						active += actual.active;
						future += a < 0 || b < 0;
						++cases;
					}
	for (int ticks : { 0, 1, 499, 500, 1999, 2000 }) {
		CObject source{};
		source.info.xSize = 297534;
		gameStates.app.nSDLTicks[0] = ticks;
		shield_effect_time = 2000;
		const auto actual = xl_compat::camera_hit_envelope({}, 297534, ticks, 2000, true);
		require(actual.needed == bool(NeedEffect(&source)) && actual.active == unsigned(ticks < 2000 ? 3 : 0), "Cleared hit timestamps changed source startup envelope");
		++cases;
	}
	for (int variant = 0; variant < 3; ++variant) {
		xl_compat::CameraHitState state;
		if (variant == 2) state.times[0] = INT32_MIN;
		bool rejected = false;
		try {
			xl_compat::camera_hit_envelope(state, variant == 0 ? 0 : 65536, INT32_MAX, variant == 1 ? 1000 : 2000, true);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Invalid hit render profile was accepted");
	}
	std::cout << "Source hit shader scalar envelope: " << cases << " cases, " << active << " active slots, " << future << " future-time profiles passed\n";
}
static void compare_hit_save()
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	unsigned cases = 0, negative_restores = 0;
	for (int type : { 4, 5 })
		for (int repaired : { -100000, 0, 1, 500, 1000000000 })
			for (int saved_ticks : { 0, 123, 10000, 1000000001 })
				for (int restored_ticks : { 0, 100, 50000, 1500000000 }) {
					const int64_t elapsed = int64_t(saved_ticks) - repaired;
					const int64_t rebased = int64_t(restored_ticks) - elapsed;
					if (elapsed < INT32_MIN || elapsed > INT32_MAX || rebased < INT32_MIN || rebased > INT32_MAX) continue;
					CObject source{};
					source.m_damage.tRepaired = repaired;
					gameStates.app.nSDLTicks[0] = saved_ticks;
					CFile file;
					source.SaveHitStateReference(file);
					xl_compat::CameraHitState native;
					native.repaired = repaired;
					const int32_t native_elapsed = native.repaired_elapsed(saved_ticks);
					require(file.words == std::vector<int32_t>{ 0, 0, 0, native_elapsed }, "Default hit-state save fields differ from source");
					CObject restored{};
					restored.m_hitInfo.i = 2;
					restored.m_hitInfo.v[0] = CFixVector::Create(1, 2, 3);
					restored.m_hitInfo.t[0] = 123;
					restored.m_damage.tShield = 234;
					restored.m_damage.tCritical = 345;
					HitStateArray<CObject> objects{ { &restored, 1 } };
					objects.Clear(); // Source PrepareLevel clears object storage before LoadState
					gameStates.app.nSDLTicks[0] = restored_ticks;
					restored.LoadHitStateReference(file);
					native.next = 2;
					native.points[0] = { 1, 2, 3 };
					native.times[0] = 123;
					native.shield_time = 234;
					native.critical_time = 345;
					native.restore_repaired(restored_ticks, native_elapsed);
					require(native.repaired == restored.m_damage.tRepaired && !native.next && !native.shield_time && !native.critical_time &&
					            !restored.m_hitInfo.i && !restored.m_damage.tShield && !restored.m_damage.tCritical,
					        "Default hit-state restore did not rebase repair time and reset omitted state");
					for (unsigned i = 0; i < 3; ++i) require(native.points[i] == point(restored.m_hitInfo.v[i]) && native.times[i] == restored.m_hitInfo.t[i], "Restored hit history differs from cleared source storage");
					negative_restores += native.repaired < 0;
					restored.info.nType = type;
					restored.info.xSize = 65537;
					restored.info.position.vPos = CFixVector::Create(317, -912, 65537);
					const auto hit = CFixVector::Create(317 + 65536, -912, 65537);
					gameStates.app.nSDLTicks[0] = restored_ticks + 1000;
					shield_effect_time = 2000;
					const auto expected = restored.RegisterHitSource(hit, 0);
					const auto actual = native.register_hit(point(hit), point(restored.info.position.vPos), 65537, type, restored_ticks + 1000, 2000);
					require(actual == point(expected) && native.repaired == restored.m_damage.tRepaired && native.shield_time == restored.m_damage.tShield && native.next == unsigned(restored.m_hitInfo.i), "First post-restore hit differs from source");
					for (unsigned i = 0; i < 3; ++i) require(native.points[i] == point(restored.m_hitInfo.v[i]) && native.times[i] == restored.m_hitInfo.t[i], "First post-restore hit history differs from source");
					++cases;
				}
	xl_compat::CameraHitState bounded;
	bounded.repaired = INT32_MIN;
	bool rejected_capture = false, rejected_restore = false;
	try {
		bounded.repaired_elapsed(INT32_MAX);
	} catch (const std::runtime_error &) {
		rejected_capture = true;
	}
	try {
		bounded.restore_repaired(INT32_MAX, INT32_MIN);
	} catch (const std::runtime_error &) {
		rejected_restore = true;
	}
	require(rejected_capture && rejected_restore && bounded.repaired == INT32_MIN, "Out-of-profile hit save arithmetic was accepted or partially published");
	require(negative_restores > 0, "Hit-state save controls omitted negative rebased repair ticks");
	std::cout << "Source hit-state save lifecycle: " << cases << " round trips, " << negative_restores << " negative repair timestamps, cleared history and first contact passed\n";
}
static void compare_sphere_unstick()
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	const auto vector = [](const Point &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	unsigned cases = 0, queries = 0, registered = 0, nonzero_results = 0;
	const int saved_model = collision_model;
	collision_model = 2;
	for (bool moved : { false, true })
		for (bool stopped : { false, true })
			for (int size : { 0, 1, 65537, 400001 })
				for (int type : { 4, 5 })
					for (int ticks : { 123, 500, 10000 })
						for (int duration : { 500, 2000 })
							for (int extent : { 0, 1, 6000001 }) {
								const Point origin{ 317, -912, 65537 }, offset{ extent, -extent / 3, extent / 7 };
								Point endpoint = origin;
								if (moved)
									for (unsigned axis = 0; axis < 3; ++axis) endpoint[axis] += offset[axis];
								const Point contact{ 65437, 227, -813 }, camera_position{ 123456, -456789, 987654 };
								const Point velocity = stopped ? Point{} : Point{ 1, -2, 3 };
								CObject actor{};
								actor.info.nType = type;
								actor.info.xSize = size;
								actor.info.position.vPos = vector(contact);
								actor.polygon = false;
								actor.apply_hit_state = true;
								actor.m_hitInfo.i = 1;
								actor.m_hitInfo.v[0] = vector({ 65537, 0, 0 });
								actor.m_hitInfo.v[1] = vector({ 0, 65537, 0 });
								actor.m_hitInfo.v[2] = vector({ 0, 0, 65537 });
								unstick_target = {};
								unstick_target.polygon = false;
								unstick_target.info.nType = 16;
								unstick_target.info.position.vPos = vector(camera_position);
								unstick_target.info.xSize = 108769;
								gameStates.app.nSDLTicks[0] = ticks;
								shield_effect_time = duration;
								CPhysSimData source{ { 0 }, vector(offset), vector(origin), vector(endpoint) };
								auto old_velocity = vector(velocity);
								unsigned source_queries = 0;
								unstick_query = [&](const CFixVector &a, const CFixVector &b) {
									++source_queries;
									auto start = a, end = b;
									CHitData hit{};
									const int result = ReferenceVectorObjectCollision(hit, &start, &end, actor.info.xSize, &actor, &unstick_target, -1, false);
									nonzero_results += result != 0;
									return result != 0;
								};
								actor.UnstickFromObject(source, old_velocity);
								xl_compat::CameraUnstickState state{ origin, endpoint, offset };
								xl_compat::CameraHitState hits;
								hits.next = 1;
								hits.points = { Point{ 65537, 0, 0 }, Point{ 0, 65537, 0 }, Point{ 0, 0, 65537 } };
								auto position = contact;
								const auto count = xl_compat::camera_sphere_unstick(state, hits, position, camera_position, velocity, size, type, ticks, duration);
								require(count == source_queries && position == point(actor.info.position.vPos) && state.new_position == point(source.vNewPos) && state.offset == point(source.vOffset), "Composed default sphere unstick differs from source");
								require(hits.repaired == actor.m_damage.tRepaired && hits.shield_time == actor.m_damage.tShield && hits.next == unsigned(actor.m_hitInfo.i), "Composed reverse query lost source hit-state updates");
								for (unsigned i = 0; i < 3; ++i) require(hits.points[i] == point(actor.m_hitInfo.v[i]) && hits.times[i] == actor.m_hitInfo.t[i], "Composed reverse query changed hit history");
								++cases;
								queries += count;
								registered += actor.registered_hits;
							}
	unstick_query = {};
	collision_model = saved_model;
	require(queries && registered && !nonzero_results, "Default reverse sphere query behavior changed");
	std::cout << "Source composed sphere unstick: " << cases << " cases, " << queries << " reverse queries, " << registered << " registrations, " << nonzero_results << " nonzero results\n";
}
static unsigned transform_cases;
static unsigned quad_cases;
static unsigned quad_hits;
static unsigned vector_cases, vector_hits;
static void compare_vector_query(CObject &actor, xl_compat::ModelHitboxTransform &cache,
                                 const std::array<xl_compat::FixedLightPoint, 3> &orientation, const xl_compat::FixedLightPoint &position)
{
	using Point = xl_compat::FixedLightPoint;
	const auto vector = [](const Point &v) { return CFixVector::Create(v[0], v[1], v[2]); };
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	for (int offset : { -524288, -65536, 0, 65536, 524288 })
		for (unsigned axis = 0; axis < 3; ++axis)
			for (int radius : { 0, 1, 16384, 65536 }) {
				Point start = position, end = position;
				start[axis] -= 16 * 65536;
				end[axis] += 16 * 65536;
				start[(axis + 1) % 3] += offset;
				end[(axis + 1) % 3] += offset;
				auto p0 = vector(start), p1 = vector(end), ref = vector(position);
				auto expected_hit = CFixVector::Create(17, 19, 23), expected_normal = CFixVector::Create(29, 31, 37);
				Point actual_hit{ 17, 19, 23 }, actual_normal{ 29, 31, 37 };
				int16_t expected_model = -7, actual_model = -7;
				const auto expected = CheckVectorHitboxCollision(expected_hit, expected_normal, &p0, &p1, &ref, &actor, radius, expected_model);
				const auto actual = xl_compat::hitbox_vector_query(actual_hit, actual_normal, actual_model, cache, orientation, position,
				                                                   gameData.objData.nFrameCount, collision_model, start, end, radius);
				require(actual == expected && actual_hit == point(expected_hit) && actual_normal == point(expected_normal) && actual_model == expected_model,
				        "Vector/hitbox nearest face, normal, submodel or miss state differs from source");
				++vector_cases;
				vector_hits += actual != INT32_MAX;
			}
}
static void compare_quad(const std::array<xl_compat::FixedLightPoint, 4> &vertices, const xl_compat::FixedLightPoint &normal)
{
	using Point = xl_compat::FixedLightPoint;
	const auto vector = [](const Point &v) { return CFixVector::Create(v[0], v[1], v[2]); };
	CFixVector source_vertices[4];
	Point center{};
	for (unsigned axis = 0; axis < 3; ++axis) center[axis] = int32_t((int64_t(vertices[0][axis]) + vertices[1][axis] + vertices[2][axis] + vertices[3][axis]) / 4);
	for (unsigned i = 0; i < 4; ++i) source_vertices[i] = vector(vertices[i]);
	auto source_normal = vector(normal);
	for (int offset : { -262144, -65536, 0, 65536, 262144 })
		for (unsigned axis = 0; axis < 3; ++axis)
			for (int shape = 0; shape < 4; ++shape)
				for (int radius : { 0, 1, 16384, 65536 }) {
					Point start = center, end = center;
					start[(axis + 1) % 3] += offset;
					end[(axis + 1) % 3] += offset;
					start[axis] -= 8 * 65536;
					end[axis] += shape == 0 ? 8 * 65536 : shape == 1 ? 0
					                                  : shape == 2   ? -8 * 65536
					                                                 : -8 * 65536 - 1;
					auto source_start = vector(start), source_end = vector(end);
					CFixVector expected = CFixVector::Create(17, 19, 23);
					Point actual{ 17, 19, 23 };
					const auto distance = FindLineQuadIntersection(expected, source_vertices, &source_normal, &source_start, &source_end, radius);
					const auto result = xl_compat::hitbox_line_quad(actual, vertices, normal, start, end, radius);
					require(result == distance && actual == Point{ expected.v.coord.x, expected.v.coord.y, expected.v.coord.z }, "Line/quad radius, intersection or miss state differs from source");
					++quad_cases;
					quad_hits += distance != INT32_MAX;
				}
}
static void compare_transforms(const std::vector<xl_compat::ModelHitboxBounds> &bounds)
{
	using Point = xl_compat::FixedLightPoint;
	using Matrix = std::array<Point, 3>;
	xl_compat::ModelHitboxTransform native(bounds);
	auto &source = gameData.modelData.hitboxes[0];
	source.nFrame = gameData.objData.nFrameCount = 0;
	for (std::size_t box = 0; box < bounds.size(); ++box)
		for (auto &face : source.hitboxes[box].box.faces) {
			face.n[1] = {};
			for (auto &vertex : face.v) vertex = {};
		}
	const auto point = [](const CFixVector &v) { return Point{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	for (const Matrix &orientation : { Matrix{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } },
	                                   Matrix{ { { 0, 65536, 0 }, { 0, 0, 65536 }, { 65536, 0, 0 } } },
	                                   Matrix{ { { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } } })
		for (const Point &position : { Point{}, Point{ 65536, -131072, 262144 }, Point{ -234567, 765432, -123456 } })
			for (int mode : { 0, 1, 2 })
				for (int repeat = 0; repeat < 3; ++repeat) {
					CObject actor{};
					actor.info.position.vPos = CFixVector::Create(position[0], position[1], position[2]);
					// Storage adapter for the audited View(0) transpose convention
					actor.view.m.dir.r = CFixVector::Create(orientation[0][0], orientation[1][0], orientation[2][0]);
					actor.view.m.dir.u = CFixVector::Create(orientation[0][1], orientation[1][1], orientation[2][1]);
					actor.view.m.dir.f = CFixVector::Create(orientation[0][2], orientation[1][2], orientation[2][2]);
					collision_model = mode;
					// Change object/position/mode while staying on the same model frame
					// on repeat0/2; only repeat1 is allowed to refresh release geometry
					if (repeat == 1) ++gameData.objData.nFrameCount;
					auto override_position = actor.info.position.vPos;
					override_position.v.coord.x += 1701;
					auto actual_position = position;
					const bool override = repeat == 2 || (repeat == 1 && mode == 2);
					if (override) actual_position[0] += 1701;
					TransformHitboxes(&actor, override ? &override_position : nullptr);
					const auto &world = native.transform(orientation, actual_position, gameData.objData.nFrameCount, mode);
					for (std::size_t box = 0; box < bounds.size(); ++box)
						for (unsigned face = 0; face < 6; ++face) {
							const auto &expected = source.hitboxes[box].box.faces[face];
							require(world[box].normals[face] == point(expected.n[1]), "World hitbox normal differs from source");
							for (unsigned vertex = 0; vertex < 4; ++vertex)
								require(world[box].faces[face][vertex] == point(expected.v[vertex]), "World hitbox vertex/cache differs from source");
							if (repeat == 1 && mode == 1 && box == 0) compare_quad(world[box].faces[face], world[box].normals[face]);
						}
					++transform_cases;
					compare_vector_query(actor, native, orientation, actual_position);
				}
}
static void word(std::vector<uint8_t> &bytes, unsigned value)
{
	bytes.push_back(uint8_t(value));
	bytes.push_back(uint8_t(value >> 8));
}
static void integer(std::vector<uint8_t> &bytes, int32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(uint32_t(value) >> shift));
}
static unsigned pair_cases, pair_hits;
static void compare_pairs(const std::vector<xl_compat::ModelHitboxBounds> &bounds)
{
	using Point = xl_compat::FixedLightPoint;
	using Matrix = std::array<Point, 3>;
	const Matrix identity{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	const auto vector = [](const Point &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	const auto point = [](const CFixVector &p) { return Point{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
	const auto set_view = [&](CObject &actor, const Matrix &m) {
		actor.view.m.dir.r = CFixVector::Create(m[0][0], m[1][0], m[2][0]);
		actor.view.m.dir.u = CFixVector::Create(m[0][1], m[1][1], m[2][1]);
		actor.view.m.dir.f = CFixVector::Create(m[0][2], m[1][2], m[2][2]);
	};
	gameData.modelData.hitboxes[1] = gameData.modelData.hitboxes[0];
	xl_compat::ModelHitboxTransform moving(bounds, -1), stationary(bounds, -1);
	for (const Matrix &orientation : { identity, Matrix{ { { 0, 65536, 0 }, { 0, 0, 65536 }, { 65536, 0, 0 } } },
	                                   Matrix{ { { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } } })
		for (int mode : { 1, 2 })
			for (unsigned axis = 0; axis < 3; ++axis)
				for (int offset : { -6, -2, -1, 0, 1, 2, 6 })
					for (int sweep : { 0, 12 })
						for (bool shared : { false, true }) {
							++gameData.objData.nFrameCount;
							collision_model = mode;
							CObject a{}, b{};
							b.model = shared ? 0 : 1;
							set_view(a, orientation);
							set_view(b, identity);
							const Point position{ 12345, -23456, 34567 };
							b.info.position.vPos = vector(position);
							Point start = position, end = position;
							end[axis] += offset * 65536;
							start = end;
							start[axis] -= sweep * 65536;
							a.info.position.vPos = vector(start);
							auto p0 = vector(start), p1 = vector(end);
							auto expected_hit = CFixVector::Create(17, 19, 23), expected_normal = CFixVector::Create(29, 31, 37);
							Point actual_hit{ 17, 19, 23 }, actual_normal{ 29, 31, 37 };
							int16_t expected_model = -7, actual_model = -7;
							const auto expected = CheckHitboxCollision(expected_hit, expected_normal, &a, &b, &p0, &p1, expected_model);
							const auto actual = xl_compat::hitbox_pair_query(actual_hit, actual_normal, actual_model, moving, orientation,
							                                                 shared ? moving : stationary, identity, position, gameData.objData.nFrameCount, mode, start, end);
							require(actual == expected && actual_hit == point(expected_hit) && actual_normal == point(expected_normal) && actual_model == expected_model,
							        "Polygon hitbox pair distance, average, normal, fallback or model differs from source");
							++pair_cases;
							pair_hits += actual != 0;
						}
}
static unsigned object_cases, object_hits, object_registrations;
static void compare_objects(const std::vector<xl_compat::ModelHitboxBounds> &bounds)
{
	using Point = xl_compat::FixedLightPoint;
	const std::array<Point, 3> identity{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	const auto vector = [](const Point &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	const auto point = [](const CFixVector &p) { return Point{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
	gameData.modelData.hitboxes[1] = gameData.modelData.hitboxes[0];
	xl_compat::ModelHitboxTransform first(bounds, -1), second(bounds, -1);
	gameData.modelData.offsets[0] = CFixVector::Create(17311, -22017, 39001);
	for (const auto &orientation : { identity, std::array<Point, 3>{ { { 0, 65536, 0 }, { 0, 0, 65536 }, { 65536, 0, 0 } } }, std::array<Point, 3>{ { { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } } })
		for (bool polygon : { false, true }) {
			CObject source{};
			source.info.renderType = polygon ? RT_POLYOBJ : 0;
			source.info.position.vPos = CFixVector::Create(12345, -23456, 34567);
			source.view.m.dir.r = CFixVector::Create(orientation[0][0], orientation[1][0], orientation[2][0]);
			source.view.m.dir.u = CFixVector::Create(orientation[0][1], orientation[1][1], orientation[2][1]);
			source.view.m.dir.f = CFixVector::Create(orientation[0][2], orientation[1][2], orientation[2][2]);
			CFixVector expected{};
			PolyObjPos(&source, &expected);
			const xl_compat::HitboxQueryActor actor{ polygon ? &first : nullptr, orientation, point(source.info.position.vPos), 65536, 65536, point(gameData.modelData.offsets[0]) };
			require(xl_compat::hitbox_actor_center(actor) == point(expected), "Rotated model center differs from source PolyObjPos");
		}
	for (int profile : { 0, 1, 2 })
		for (int mode : { 1, 2 })
			for (unsigned axis = 0; axis < 3; ++axis)
				for (int offset : { -40, -6, -1, 0, 1, 6, 40 })
					for (int sweep : { 0, 12 })
						for (bool visibility : { false, true }) {
							collision_model = mode;
							++gameData.objData.nFrameCount;
							CObject stationary{}, moving{};
							moving.model = 1;
							stationary.polygon = profile != 2;
							moving.polygon = profile != 1;
							stationary.info.xSize = 65537;
							moving.info.xSize = 32769;
							for (auto *actor : { &stationary, &moving }) {
								actor->view.m.dir.r = vector(identity[0]);
								actor->view.m.dir.u = vector(identity[1]);
								actor->view.m.dir.f = vector(identity[2]);
							}
							const Point position{ 12345, -23456, 34567 };
							Point start = position, end = position;
							end[axis] += offset * 65536;
							start = end;
							start[axis] -= sweep * 65536;
							stationary.info.position.vPos = vector(position);
							moving.info.position.vPos = vector(start);
							auto p0 = vector(start), p1 = vector(end);
							CHitData data{};
							const auto expected = ReferenceVectorObjectCollision(data, &p0, &p1, moving.info.xSize, &stationary, &moving, -1, visibility);
							Point hit{}, normal{};
							int16_t model = -1;
							const xl_compat::HitboxQueryActor a{ stationary.polygon ? &first : nullptr, identity, position, stationary.info.xSize, stationary.model_radius, point(gameData.modelData.offsets[0]) };
							const xl_compat::HitboxQueryActor b{ moving.polygon ? &second : nullptr, identity, start, moving.info.xSize, moving.model_radius };
							xl_compat::HitboxRegistration registration;
							const auto actual = xl_compat::hitbox_object_query(hit, normal, model, a, b, gameData.objData.nFrameCount, mode, start, end, &registration);
							require(actual == expected, "Outer object query distance differs from source");
							if (actual) require(hit == point(data.vPoint) && normal == point(data.vNormal), "Outer object contact differs from source");
							if (visibility) require(stationary.registered_hits == 0, "Visibility query registered actor damage");
							else if (expected) require(stationary.registered_hits == 1, "Contact query omitted actor registration");
							require(stationary.registered_hits == unsigned(!visibility && registration.reached), "Outer query registration reachability differs from source");
							if (stationary.registered_hits) require(registration.point == point(stationary.registered_point) && registration.model == stationary.registered_model, "Outer query registration arguments differ from source");
							++object_cases;
							object_hits += actual != 0;
							object_registrations += stationary.registered_hits;
						}
}
static int compare(const std::vector<uint8_t> &bytes)
{
	const xl_compat::ModelRenderGeometry render(bytes.data(), bytes.size());
	pof_reference::compare(bytes, render);
	RenderCount source_count;
	uint16_t submodels = 1, vertices = 0, faces = 0, face_vertices = 0;
	require(source_count.CountPOFModelItems(const_cast<uint8_t *>(bytes.data()), &submodels, &vertices, &faces, &face_vertices) == 1, "Source POF count rejected payload");
	require(render.parts.size() == submodels && render.vertex_count() == vertices && render.face_count == faces && render.face_vertex_count == face_vertices, "POF render counts differ from source");
	std::cout << "Source POF render counts: " << submodels << " parts, " << vertices << " vertices, " << faces << " faces, " << face_vertices << " face vertices\n";
	const xl_compat::ModelGeometry geometry(bytes.data(), bytes.size());
	const int actual = geometry.custom_radius();
	for (bool direction : { false, true }) {
		const auto bounds = xl_compat::ModelGeometry(bytes.data(), bytes.size(), 10, direction).hitbox_bounds();
		facing = direction;
		CPolyModel source;
		source.data = bytes;
		require(source.Size() == actual, "HXM custom radius differs from extracted XL functions");
		const auto &reference = gameData.modelData.hitboxes[0];
		require(bounds.size() == std::size_t(reference.nHitboxes + 1), "HXM hitbox count differs from source");
		for (std::size_t i = 0; i < bounds.size(); ++i) {
			const auto &box = reference.hitboxes[i];
			const auto vector = [](const CFixVector &v) { return std::array<int32_t, 3>{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
			const auto constructed = xl_compat::model_hitbox(bounds[i]);
			for (unsigned vertex = 0; vertex < 8; ++vertex)
				require(constructed.vertices[vertex] == vector(box.box.vertices[vertex]), "Hitbox vertex differs from source ComputeHitbox");
			for (unsigned face = 0; face < 6; ++face)
				require(constructed.normals[face] == vector(box.box.faces[face].n[0]), "Hitbox face normal differs from source ComputeHitbox");
			const std::array<std::array<int32_t, 3>, 4> native{ bounds[i].low, bounds[i].high, bounds[i].offset, bounds[i].half_size };
			const std::array<std::array<int32_t, 3>, 4> expected{ vector(box.vMin), vector(box.vMax), vector(box.vOffset), vector(box.vSize) };
			for (unsigned field = 0; field < native.size(); ++field)
				for (unsigned axis = 0; axis < 3; ++axis)
					require(native[field][axis] == expected[field][axis], "HXM hitbox " + std::to_string(i) + " field " + std::to_string(field) +
					                                                          " axis " + std::to_string(axis) + " native " + std::to_string(native[field][axis]) + " source " + std::to_string(expected[field][axis]));
		}
		compare_transforms(bounds);
		compare_pairs(bounds);
		compare_objects(bounds);
	}
	if (std::all_of(render.parts.begin(), render.parts.end(), [](const auto &part) { return part.faces != 0; })) {
		const auto bounds = render.hitbox_bounds();
		gameData.modelData.hitboxes[0] = pof_reference::gameData.modelData.hitboxes[0];
		compare_transforms(bounds);
		compare_pairs(bounds);
		compare_objects(bounds);
		std::cout << "Source post-build POF collision queries matched\n";
	}
	return actual;
}
static std::vector<uint8_t> points()
{
	std::vector<uint8_t> bytes;
	word(bytes, OP_DEFP_START);
	word(bytes, 4);
	word(bytes, 0);
	word(bytes, 0);
	for (int value : { 0, -65536, 65536, 0 })
		for (int axis = 0; axis < 3; ++axis) integer(bytes, value * (axis + 1));
	word(bytes, OP_EOF);
	return bytes;
}
static unsigned read32(const std::vector<uint8_t> &bytes, std::size_t &at)
{
	require(at <= bytes.size() && bytes.size() - at >= 4, "HXM test input is truncated");
	unsigned result = 0;
	for (unsigned shift = 0; shift < 32; shift += 8) result |= unsigned(bytes[at++]) << shift;
	return result;
}
static void original_models(const char *path, const std::string &selected)
{
	std::ifstream file(path, std::ios::binary);
	require(bool(file), "Original HXM archive is unavailable");
	const xl_compat::Bytes hog{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	unsigned tested = 0;
	for (const auto &member : xl_compat::read_hog(hog)) {
		std::string name = member.name;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		if (name != selected) continue;
		const auto &bytes = member.bytes;
		std::size_t at = 0;
		require(read32(bytes, at) == 0x21584d48 && read32(bytes, at) == 1, "Original HXM header changed");
		const auto robots = read32(bytes, at);
		at += robots * 484;
		const auto joints = read32(bytes, at);
		at += joints * 12;
		const auto models = read32(bytes, at);
		for (unsigned model = 0; model < models; ++model) {
			const auto id = read32(bytes, at);
			auto header = at + 4;
			const auto size = read32(bytes, header);
			at += 734;
			require(at <= bytes.size() && size <= bytes.size() - at, "Original HXM model truncated");
			const std::vector<uint8_t> payload(bytes.begin() + at, bytes.begin() + at + size);
			std::cout << selected << " model " << id << " custom radius " << compare(payload) << '\n';
			at += size + 8;
			++tested;
		}
	}
	require(tested == 1, "Original selected HXM model count changed");
}
void compare_robot_animation(const char *path);
void compare_player_starts(const char *path);
int main(int argc, char **argv)
{
	try {
		if (argc == 3 && std::string(argv[1]) == "--starts") {
			compare_player_starts(argv[2]);
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--animation") {
			compare_robot_animation(argv[2]);
			return 0;
		}
		collision_profile_reference::compare();
		compare_sphere();
		compare_unstick();
		compare_hit_state();
		compare_hit_save();
		compare_hit_render();
		compare_hit_vectors();
		compare_shield_sphere();
		compare_shield_pulse();
		compare_player_shield();
		compare_addon_animation();
		compare_sphere_unstick();
		unsigned closest_cases = 0;
		for (int extent : { 0, 1, 65536, 983041, -524287 })
			for (int x : { -1048576, -65537, -1, 0, 1, 65537, 1048576 })
				for (int y : { -1048576, -65537, -1, 0, 1, 65537, 1048576 }) {
					const auto a = CFixVector::Create(317, -912, 65537), b = CFixVector::Create(317 + extent, -912 + extent / 3, 65537 - extent / 7), p = CFixVector::Create(x, y, -y / 2);
					CFixVector expected{};
					FindPointLineIntersection(expected, a, b, p, 0);
					const auto point = [](const CFixVector &v) { return xl_compat::FixedLightPoint{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
					const auto actual = xl_compat::hitbox_closest_point(point(a), point(b), point(p));
					require(actual == point(expected), "Finite-segment closest point differs from source");
					require(xl_compat::geometry_default_magnitude(xl_compat::hitbox_difference(actual, point(p))) == CFixVector::Dist(expected, p), "Finite-segment distance differs from source");
					++closest_cases;
				}
		std::cout << "Source finite-segment broadphase: " << closest_cases << " cases\n";
		const auto cloud = points();
		compare(cloud);
		auto surfaced = cloud;
		surfaced.resize(surfaced.size() - 2);
		const auto face_at = surfaced.size();
		word(surfaced, OP_FLATPOLY);
		word(surfaced, 3);
		surfaced.resize(surfaced.size() + 26);
		for (unsigned index : { 0u, 1u, 2u }) word(surfaced, index);
		word(surfaced, OP_EOF);
		compare(surfaced);
		for (int failure = 0; failure < 3; ++failure) {
			auto malformed = surfaced;
			if (failure == 0) malformed[face_at + 30] = 99;
			else if (failure == 1) malformed.resize(5);
			else {
				malformed.assign(32, 0);
				malformed[0] = OP_SORTNORM;
			}
			bool rejected = false;
			try {
				xl_compat::ModelRenderGeometry invalid(malformed.data(), malformed.size());
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Malformed POF render geometry was accepted");
		}
		auto nested = cloud;
		nested.resize(nested.size() - 2);
		word(nested, OP_SUBCALL);
		word(nested, 1);
		for (int offset : { 131072, -65536, 262144 }) integer(nested, offset);
		word(nested, 22);
		word(nested, 0);
		word(nested, OP_EOF);
		nested.insert(nested.end(), cloud.begin(), cloud.end());
		compare(nested);
		auto nested_faces = surfaced;
		nested_faces.resize(nested_faces.size() - 2);
		word(nested_faces, OP_SUBCALL);
		word(nested_faces, 1);
		for (int offset : { 131072, -65536, 262144 }) integer(nested_faces, offset);
		word(nested_faces, 22);
		word(nested_faces, 0);
		word(nested_faces, OP_EOF);
		nested_faces.insert(nested_faces.end(), surfaced.begin(), surfaced.end());
		const xl_compat::ModelRenderGeometry nested_render(nested_faces.data(), nested_faces.size());
		require(nested_render.parts.size() == 2 && nested_render.face_count == 2, "Nested face fixture did not reach its child");
		compare(nested_faces);
		for (int variant = 0; variant < 2; ++variant) {
			auto bad = nested;
			const auto subcall = cloud.size() - 2;
			if (!variant) bad[subcall + 2] = 10;
			else {
				for (int byte = 0; byte < 4; ++byte) bad[subcall + 4 + byte] = uint8_t(uint32_t(INT32_MAX) >> (byte * 8));
			}
			bool rejected = false;
			try {
				xl_compat::ModelGeometry(bad.data(), bad.size()).custom_radius();
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Invalid animation submodel or transformed coordinate accepted");
		}
		auto sorted = std::vector<uint8_t>(32, 0);
		sorted[0] = OP_SORTNORM;
		sorted[28] = 34;
		sorted[30] = uint8_t(34 + cloud.size());
		word(sorted, OP_EOF);
		sorted.insert(sorted.end(), cloud.begin(), cloud.end());
		sorted.insert(sorted.end(), cloud.begin(), cloud.end());
		compare(sorted);
		for (auto bad : { std::vector<uint8_t>{}, std::vector<uint8_t>{ 99, 0 }, std::vector<uint8_t>{ OP_SUBCALL, 0 } }) {
			bool rejected = false;
			try {
				xl_compat::ModelGeometry geometry(bad.data(), bad.size());
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Malformed model geometry accepted");
		}
		// XL's else-if extrema updates leave max uninitialized for decreasing points
		// The bounded reader defines both extrema, rather than reproducing overflow
		std::vector<uint8_t> monotone;
		word(monotone, OP_DEFPOINTS);
		word(monotone, 2);
		for (int value : { 65536, -65536 })
			for (int axis = 0; axis < 3; ++axis) integer(monotone, value);
		word(monotone, OP_EOF);
		require(xl_compat::ModelGeometry(monotone.data(), monotone.size()).custom_radius() == 92682, "Monotone vertex correction changed");
		std::vector<uint8_t> minimum;
		word(minimum, OP_DEFPOINTS);
		word(minimum, 2);
		for (int value : { INT32_MIN, INT32_MIN + 2 })
			for (int axis = 0; axis < 3; ++axis) integer(minimum, value);
		word(minimum, OP_EOF);
		require(xl_compat::ModelGeometry(minimum.data(), minimum.size()).custom_radius() == 2, "Minimum signed coordinate extrema changed");
		if (argc > 2 && std::string(argv[1]) == "--model") {
			std::ifstream model(argv[2], std::ios::binary);
			require(bool(model), "Native model payload is unavailable");
			const std::vector<uint8_t> bytes{ std::istreambuf_iterator<char>(model), std::istreambuf_iterator<char>() };
			std::cout << "Native model payload custom radius " << compare(bytes) << '\n';
		} else if (argc > 1) original_models(argv[1], argc > 2 ? argv[2] : "alarlof.hxm");
		std::cout << "HXM extracted-source geometry, nested offsets, sort branches and bounded malformed controls passed\n";
		std::cout << "HXM world transform/cache comparisons: " << transform_cases << " passed\n";
		require(quad_hits && quad_hits < quad_cases, "Line/quad controls must exercise hits and misses");
		std::cout << "HXM line/quad comparisons: " << quad_cases << " passed, hits " << quad_hits << '\n';
		require(vector_hits && vector_hits < vector_cases, "Vector/hitbox controls must exercise hits and misses");
		std::cout << "HXM vector/hitbox comparisons: " << vector_cases << " passed, hits " << vector_hits << '\n';
		require(pair_hits && pair_hits < pair_cases, "Polygon hitbox controls must exercise hits and misses");
		std::cout << "HXM polygon/hitbox comparisons: " << pair_cases << " passed, hits " << pair_hits << '\n';
		std::cout << "HXM outer object comparisons: " << object_cases << " passed, hits " << object_hits << ", registration calls " << object_registrations << '\n';
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
