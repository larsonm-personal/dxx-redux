#include "particle_geometry.h"
#include "particle_fire.h"
#include <cstring>
#include <iostream>
#include <optional>

template <typename T>
T Min(T a, T b)
{
	return std::min(a, b);
}
namespace particle_geometry_reference
{
using fix = std::int32_t;
using fixang = std::int16_t;
#include "xl_reference_particle_math.h"
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
	static CFixVector Create(int x, int y, int z)
	{
		CFixVector result;
		result.Set(x, y, z);
		return result;
	}
	CFixVector operator-(const CFixVector &b) const
	{
		return { { { v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y, v.coord.z - b.v.coord.z } } };
	}
	CFixVector operator*(int scale) const
	{
		return { { { FixMul(v.coord.x, scale), FixMul(v.coord.y, scale), FixMul(v.coord.z, scale) } } };
	}
	CFixVector &operator+=(const CFixVector &b)
	{
		v.coord.x += b.v.coord.x;
		v.coord.y += b.v.coord.y;
		v.coord.z += b.v.coord.z;
		return *this;
	}
	CFixVector operator+(const CFixVector &b) const
	{
		auto result = *this;
		result += b;
		return result;
	}
	CFixVector &operator*=(int scale)
	{
		return *this = *this * scale;
	}
	void SetZero()
	{
		Set(0, 0, 0);
	}
	fix Mag() const;
	void Check();
	static const CFixVector Perp(const CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Normal(const CFixVector &, const CFixVector &, const CFixVector &);
	static const fix Dist(const CFixVector &, const CFixVector &);
	const fix DistToPlane(const CFixVector &, const CFixVector &) const;
	static const fix Dot(fix, fix, fix, const CFixVector &);
	static const CFixVector Cross(const CFixVector &, const CFixVector &);
	static const CFixVector Avg(const CFixVector &, const CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	static const fix NormalizedDir(CFixVector &, const CFixVector &, const CFixVector &);
	static const fixang DeltaAngleNorm(const CFixVector &, const CFixVector &, CFixVector *);
	static const fixang DeltaAngle(const CFixVector &, const CFixVector &, CFixVector *);
};
struct CFloatVector {
	union {
		struct {
			float x, y, z, w;
		} coord;
		struct {
			float r, g, b, a;
		} color;
		float vec[4];
	} v{};
	void Set(float x, float y, float z)
	{
		v.coord = { x, y, z, 0 };
	}
	float &Red()
	{
		return v.color.r;
	}
	float &Green()
	{
		return v.color.g;
	}
	float &Blue()
	{
		return v.color.b;
	}
	float &Alpha()
	{
		return v.color.a;
	}
	static const CFloatVector Min(CFloatVector &, float);
	static const CFloatVector Cross(const CFloatVector &, const CFloatVector &);
	const float SqrMag() const;
	const float Mag() const;
	CFloatVector &Assign(const CFixVector &);
};
struct {
	struct {
		std::vector<CFixVector> vertices;
	} segData;
} gameData;
struct CSegment {
	std::uint16_t m_vertices[8]{};
	float FaceSize(std::uint8_t);
};
struct CSide {
	int m_nCorners = 4;
	std::uint16_t m_corners[4]{};
	void SetupCorners(std::uint16_t *, std::uint16_t *);
	CFixVector *GetCornerVertices(CFixVector *);
};
#define SEGMENT_SIDE_COUNT 6
#define VERTICES           gameData.segData.vertices

struct RandomInput {
	std::vector<int> values;
	unsigned position = 0;
	bool generated = false;
	std::uint32_t state = 1;
	std::vector<int> trace;
	std::uint32_t next()
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	float unit()
	{
		trace.push_back(-1);
		return float(next()) / float(UINT32_MAX);
	}
	int bounded(int bound)
	{
		return (*this)(bound);
	}
	int operator()(int bound)
	{
		trace.push_back(bound);
		if (generated) return int(next() % unsigned(bound));
		if (position >= values.size()) throw std::runtime_error("Particle geometry source exhausted random input");
		const int result = values[position++];
		if (result < 0 || result >= bound) throw std::runtime_error("Particle geometry source changed random draw order");
		return result;
	}
} source_random;
int Rand(int bound)
{
	return source_random(bound);
}
int RandShort()
{
	return Rand(32768);
}
#include "xl_reference_particle_geometry.h"
#include "xl_reference_particle_constants.h"

struct {
	struct {
		int nMathFormat = 2;
		struct {
			int bDisperse = 1, bSyncSizes = 0, nSize[5]{}, nAlpha[5]{}, nQuality = 2, bWobbleBubbles = 1, bCollisions = 1;
		} particles;
	} render;
	struct {
		int bExpertMode = 0;
	} app;
} options;
auto *gameOpts = &options;
struct tQuadInt {
	std::uint32_t low;
	std::int32_t high;
};
#include "xl_reference_particle_integer.h"
std::int64_t mul64(fix a, fix b)
{
	return std::int64_t(a) * b;
}
#undef XRound
#include "xl_reference_audio_geometry.h"
#include "xl_reference_particle_normal.h"

// Matrix creation is not a fire billboard consumer; capture only its RNG calls
struct CAngleVector {
	struct {
		struct {
			int p, b, h;
		} coord;
	} v{};
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u, f;
		} dir;
	} m{};
	static CFixMatrix Create(const CAngleVector &)
	{
		return {};
	}
	CFixMatrix operator*(const CFixMatrix &) const
	{
		return *this;
	}
};
float RandFloat(float scale = 1.0f)
{
	return source_random.unit() / scale;
}
int unavailable_size(int, float, int)
{
	throw std::runtime_error("Unexpected non-authored fire size");
}
#define PARTICLE_SIZE(a, b, c) unavailable_size(a, b, c)
struct ParticleSegment {
	int function = 0;
	int Function() const
	{
		return function;
	}
	bool HasFunction(int f) const
	{
		return function == f;
	}
	bool HasWaterProp() const
	{
		return false;
	}
	bool HasLavaProp() const
	{
		return false;
	}
} particle_segment;
ParticleSegment *SEGMENT(int)
{
	return &particle_segment;
}
struct {
	int GetType(int type) const
	{
		return type;
	}
} particleImageManager;
struct ImageInfo {
	int nFrames = 2;
} image_info;
ImageInfo &ParticleImageInfo(int type)
{
	if (type != 5) throw std::runtime_error("Unexpected fire image");
	return image_info;
}
struct CParticle {
	CFixVector m_vPos{}, m_vStartPos{}, m_vDrift{}, m_vDir{};
	CFixMatrix m_mOrient{};
	CFloatVector m_color[2]{}, m_renderColor{};
	struct {
		struct {
			float u = 0, v = 0;
		} v;
	} m_texCoord;
	int m_bChecked = 0, m_bBlowUp = 0, m_nType = 0, m_nClass = 0, m_nFadeType = 0, m_nSegment = 0, m_bSkybox = 0, m_nBounce = 0, m_bReversed = 0;
	int m_nUpdated = 0, m_nMoved = 0, m_nLife = 0, m_nTTL = 0, m_nDelay = 0, m_nRenderType = 0, m_bEmissive = 0, m_bBright = 0, m_nFadeTime = 0, m_bHaveDir = 0;
	int m_nFrames = 0, m_iFrame = 0, m_nRotFrame = 0, m_nOrient = 0, m_bAnimate = 0, m_bRotate = 0, m_nDelayPosUpdate = 0;
	float m_nRad = 0, m_nWidth = 0, m_nHeight = 0, m_deltaUV = 0, m_decay = 0;
	int32_t Create(CFixVector *, CFixVector *, CFixMatrix *, int16_t, int32_t, int32_t, char, char, float, CFloatVector *, int32_t, int32_t, char, float, CFixVector *);
	void UpdateDecay();
	void UpdateTexCoord();
	int32_t SetupColor(float);
	int32_t RenderType();
	int32_t ChangeDir(int32_t);
	fix Drag();
	int32_t Bounce(int32_t);
	int32_t CollideWithWall(int32_t);
	int32_t UpdateDrift(int32_t, int32_t);
	void UpdateColor(float, int32_t);
	int32_t Update(int32_t, float, int32_t);
};
struct WorldInput {
	int located = 0, portal = -1, lookups = 0, collisions = 0;
	std::optional<xl_compat::ParticlePoint> normal;
	int locate(const xl_compat::ParticlePoint &, bool, int)
	{
		++lookups;
		return located;
	}
	std::optional<xl_compat::ParticlePoint> collide(xl_compat::FireParticle &particle)
	{
		++collisions;
		if (particle.segment < 0) return {};
		if (portal >= 0) {
			particle.segment = portal;
			return {};
		}
		return normal;
	}
} source_world;
int FindSegByPosExhaustive(const CFixVector &, int, int)
{
	++source_world.lookups;
	return source_world.located;
}
CFixVector source_normal;
CFixVector *wallNorm[1] = { &source_normal };
int32_t CParticle::CollideWithWall(int32_t)
{
	++source_world.collisions;
	if (m_nSegment < 0) return 0;
	if (source_world.portal >= 0) {
		m_nSegment = source_world.portal;
		return 0;
	}
	if (!source_world.normal) return 0;
	const auto &normal = *source_world.normal;
	source_normal.Set(normal[0], normal[1], normal[2]);
	return 1;
}
#define F2X(f)        (fix((f) * 65536))
#define ENABLE_UPDATE 1
#include "xl_reference_particle_create.h"

CFixVector point(const xl_compat::ParticlePoint &p)
{
	return { { { p[0], p[1], p[2] } } };
}
xl_compat::ParticlePoint point(const CFixVector &p)
{
	return { p.v.coord.x, p.v.coord.y, p.v.coord.z };
}

void compare_sampling(const xl_compat::ParticleFace &face)
{
	CFixVector corners[4];
	for (unsigned i = 0; i < 4; ++i) corners[i] = i < face.count ? point(face.corners[i]) : point(xl_compat::ParticlePoint{ INT32_MAX, INT32_MAX, INT32_MAX });
	for (int choice : { 0, 1 })
		for (int a : { 0, 1, 8192, 16384, 16385, 32766, 32767 })
			for (int b : { 0, 1, 8192, 16384, 16385, 32766, 32767 }) {
				RandomInput actual_random;
				if (face.count == 4) actual_random.values.push_back(choice);
				actual_random.values.push_back(a);
				actual_random.values.push_back(b);
				source_random = actual_random;
				const auto actual = xl_compat::particle_face_point(face, actual_random);
				CFixVector expected;
				if (!RandomPointOnQuad(corners, &expected) || point(expected) != actual || source_random.position != actual_random.position || actual_random.position != actual_random.values.size())
					throw std::runtime_error("Actual source particle face sampling differs");
			}
}

void bind(const xl_compat::Inventory &level, int segment, CSegment &source)
{
	gameData.segData.vertices.clear();
	for (const auto &p : level.vertices) gameData.segData.vertices.push_back(point(p));
	for (unsigned i = 0; i < 8; ++i) source.m_vertices[i] = std::uint16_t(level.segments.at(segment).vertices[i]);
}

float compare_face(const xl_compat::Inventory &level, int segment, int side)
{
	CSegment source;
	bind(level, segment, source);
	const auto density = xl_compat::particle_face(level, segment, side, true);
	const float actual = xl_compat::particle_face_size(density), expected = source.FaceSize(std::uint8_t(side));
	if (actual != expected) throw std::runtime_error("Actual source particle face area differs");
	const auto emission = xl_compat::particle_face(level, segment, side, false);
	CSide source_side;
	source_side.m_nCorners = int(emission.count);
	std::uint16_t slots[4];
	for (unsigned i = 0; i < 4; ++i) slots[i] = std::uint16_t(level.segments[segment].sides[side].corners[i]);
	source_side.SetupCorners(source.m_vertices, slots);
	CFixVector corners[4]{};
	source_side.GetCornerVertices(corners);
	for (unsigned i = 0; i < emission.count; ++i)
		if (point(corners[i]) != emission.corners[i]) throw std::runtime_error("Actual source particle emission corners differ");
	compare_sampling(emission);
	return actual;
}
} // namespace particle_geometry_reference

int particle_source_plane_distance(const xl_compat::ParticlePoint &p, const xl_compat::ParticlePoint &normal, const xl_compat::ParticlePoint &anchor)
{
	using namespace particle_geometry_reference;
	return point(p).DistToPlane(point(normal), point(anchor));
}
int particle_source_distance(const xl_compat::ParticlePoint &a, const xl_compat::ParticlePoint &b)
{
	using namespace particle_geometry_reference;
	return CFixVector::Dist(point(a), point(b));
}
xl_compat::ParticlePoint particle_source_normal(const xl_compat::ParticlePoint &a, const xl_compat::ParticlePoint &b, const xl_compat::ParticlePoint &c)
{
	using namespace particle_geometry_reference;
	options.render.nMathFormat = 0;
	const auto normal = CFixVector::Normal(point(a), point(b), point(c));
	options.render.nMathFormat = 2;
	return point(normal);
}

namespace particle_reference
{
void compare(const std::vector<int> &, int, float, bool);
}

void particle_geometry_source_controls()
{
	xl_compat::Inventory level{};
	level.level_version = 27;
	level.segments.resize(1);
	level.vertices.resize(8);
	for (int i = 0; i < 8; ++i) level.segments[0].vertices[i] = i;
	unsigned faces = 0;
	for (int shape = 0; shape < 8; ++shape) {
		for (int i = 0; i < 8; ++i) {
			const int x = (i & 1) ? 655360 : -655360;
			const int y = (i & 2) ? 655360 : -655360;
			const int z = (i & 4) ? 655360 : -655360;
			level.vertices[i] = { x + shape * 331 * i, y - shape * 733 * i, z + shape * 1009 * i };
		}
		for (int side = 0; side < 6; ++side) {
			auto &slots = level.segments[0].sides[side].corners;
			slots = xl_compat::classic_side_corners[side];
			// Authored emission order differs from FaceSize's fixed classic order
			if (shape & 1) std::swap(slots[1], slots[3]);
			particle_geometry_reference::compare_face(level, 0, side);
			++faces;
			if (shape & 2) {
				slots[3] = 255;
				particle_geometry_reference::compare_face(level, 0, side);
				++faces;
			}
		}
	}
	const xl_compat::ParticleFace square{ { { { 0, 0, 0 }, { 1310720, 0, 0 }, { 1310720, 1310720, 0 }, { 0, 1310720, 0 } } }, 4 };
	if (xl_compat::particle_face_size(square) != 1.0f) throw std::runtime_error("XL particle density unit changed");
	std::cout << "Pinned particle geometry: " << faces << " faces, " << faces * 98 << " surface samples and 20-unit density scale passed\n";
}

void particle_fire_source_controls()
{
	using namespace particle_geometry_reference;
	const xl_compat::ParticleFace face{ { { { -655360, -655360, 13 }, { 655360, -655360, -19 }, { 655360, 655360, 27 }, { -655360, 655360, -35 } } }, 4 };
	unsigned count = 0, updates = 0;
	for (int value : { 1, 2, 3, 5, 8, 10 })
		for (const auto &forward : std::array<xl_compat::ParticlePoint, 3>{ { { 0, 0, 65536 }, { -17031, 10000, 60000 }, { 0, 0, 0 } } })
			for (bool on_face : { false, true })
				for (bool skybox : { false, true })
					for (unsigned seed = 0; seed < 32; ++seed) {
						xl_compat::Object authored{};
						authored.render = 11;
						authored.render_data = { value, value, value, value, 1, 10, 255, 128, 0, 128, on_face ? 1 : 0, 3, 1 };
						const auto definition = xl_compat::particle_emitter_definition(xl_compat::decode_particle_effect(authored, 27), forward, true);
						RandomInput random;
						random.generated = true;
						random.state = seed;
						source_random = random;
						const xl_compat::ParticlePoint position{ 112233, -445566, 778899 };
						const auto actual = xl_compat::create_fire_particle(definition, position, on_face ? &face : nullptr, 0, skybox, 123, random);
						CParticle source;
						auto source_position = point(position), direction = point(definition.direction);
						CFixVector corners[4];
						for (unsigned i = 0; i < 4; ++i) corners[i] = point(face.corners[i]);
						CFixMatrix orientation;
						CFloatVector color;
						for (unsigned i = 0; i < 4; ++i) color.v.vec[i] = definition.color[i];
						particle_segment.function = skybox ? SEGMENT_FUNC_SKYBOX : 0;
						if (!source.Create(&source_position, &direction, &orientation, 0, definition.life, definition.speed, 5, 0, definition.scale, &color, 123, 0, 3, 1.0f, on_face ? corners : nullptr))
							throw std::runtime_error("Actual source rejected an authored fire particle");
						if (actual.position != point(source.m_vPos) || actual.start != point(source.m_vStartPos) || actual.drift != point(source.m_vDrift) || actual.direction != point(source.m_vDir) ||
						    actual.life != source.m_nLife || actual.ttl != source.m_nTTL || actual.updated != source.m_nUpdated || actual.moved != source.m_nMoved || actual.segment != source.m_nSegment ||
						    actual.frame != source.m_iFrame || actual.rotation != source.m_nRotFrame || actual.orientation != source.m_nOrient || actual.radius != source.m_nRad || actual.width != source.m_nWidth || actual.height != source.m_nHeight || actual.decay != source.m_decay ||
						    actual.bounce != source.m_nBounce || actual.locate_delay != source.m_nDelayPosUpdate || actual.skybox != bool(source.m_bSkybox) || random.state != source_random.state || random.trace != source_random.trace)
							throw std::runtime_error("Actual source fire creation differs at seed " + std::to_string(seed) + " value " + std::to_string(value));
						for (unsigned i = 0; i < 4; ++i)
							if (source.m_renderColor.v.vec[i] != 1.0f) throw std::runtime_error("Source fire lost its white additive color");
						if (source.m_texCoord.v.u != 0 || source.m_texCoord.v.v != float(actual.frame / 2) * 0.5f || source.m_bAnimate || source.m_bBlowUp || source.m_bEmissive != 2)
							throw std::runtime_error("Source fire atlas or emission policy differs");
						if (seed < 8)
							for (int scenario = 0; scenario < 6; ++scenario) {
								auto moving = actual;
								auto reference = source;
								auto moving_random = random;
								source_random = random;
								WorldInput world;
								world.located = scenario == 1 ? -1 : 0;
								if (scenario == 2 || scenario == 3) {
									world.normal = xl_compat::ParticlePoint{ scenario == 2 ? 65536 : -65536, 0, 0 };
									moving.drift = { 655360, 0, 0 };
									moving.direction = { 0, 0, 0 };
									reference.m_vDrift = point(moving.drift);
									reference.m_vDir = point(moving.direction);
								}
								if (scenario == 5) world.portal = 1;
								source_world = world;
								gameOpts->render.particles.bCollisions = scenario != 4;
								for (int offset : { 0, 1, 16, 249, 250, 251, 500, 501, 750, 1000, 2000, 6000 }) {
									const bool result = xl_compat::advance_fire_particle(moving, 123 + offset, moving_random, world, scenario != 4);
									const bool expected = reference.Update(123 + offset, 1.0f, 0) != 0;
									if (result != expected || moving.position != point(reference.m_vPos) || moving.drift != point(reference.m_vDrift) || moving.direction != point(reference.m_vDir) || moving.life != reference.m_nLife || moving.decay != reference.m_decay ||
									    moving.updated != reference.m_nUpdated || moving.moved != reference.m_nMoved || moving.segment != reference.m_nSegment || moving.bounce != reference.m_nBounce || moving.locate_delay != reference.m_nDelayPosUpdate ||
									    moving_random.state != source_random.state || moving_random.trace != source_random.trace || world.lookups != source_world.lookups || world.collisions != source_world.collisions)
										throw std::runtime_error("Actual source fire update differs at scenario " + std::to_string(scenario) + " offset " + std::to_string(offset));
									++updates;
								}
							}
						++count;
					}
	std::cout << "Pinned authored fire: " << count << " creation and " << updates << " update comparisons with vector math, RNG order, lifetime, size, atlas and scripted world consumers passed\n";
}

void particle_original_geometry_controls(const xl_compat::Inventory &level)
{
	unsigned count = 0;
	for (const auto &object : level.objects) {
		if (object.render != 11 || object.render_data.size() != 13 || object.render_data[10] <= 0) continue;
		const float area = particle_geometry_reference::compare_face(level, object.segment, object.render_data[10] - 1);
		// Modern explicit spray subtype does not consult legacy water textures
		if (level.level_version >= 18) particle_reference::compare(object.render_data, level.level_version, area, false);
		++count;
	}
	std::cout << "Original particle faces: " << count << " density and emission geometries compared\n";
}
