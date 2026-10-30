// Geometry queries are explicit inputs; the actual pinned conversion is executed
#include "particle_effect.h"
#include <iostream>
#include <vector>
namespace particle_reference
{
using fix = std::int32_t;
#include "xl_reference_particle_math.h"
struct CFloatVector {
	struct {
		struct {
			float r, g, b, a;
		} color;
	} v;
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
};
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v;
	CFixVector operator*(int scale) const
	{
		return { { { FixMul(v.coord.x, scale), FixMul(v.coord.y, scale), FixMul(v.coord.z, scale) } } };
	}
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector f;
		} dir;
	} m;
};
struct ByteColor {
	std::array<int, 4> values{};
	int Red() const
	{
		return values[0];
	}
	int Green() const
	{
		return values[1];
	}
	int Blue() const
	{
		return values[2];
	}
	int Alpha() const
	{
		return values[3];
	}
};
struct tParticleInfo {
	int nLife, nSize[2], nParts, nSpeed, nDrift, nBrightness, nSide, nType;
	int bEnabled = 1;
	ByteColor color;
};
struct CObject {
	struct {
		int nType = 20, nId = 0, renderType = 11, nSegment = 0;
		struct {
			CFixVector vPos{};
			CFixMatrix mOrient{};
		} position;
	} info;
	struct {
		tParticleInfo particleInfo{};
	} rType;
	void SetupSmoke();
	int Index() const
	{
		return 0;
	}
};
struct {
	struct {
		int nLevelVersion = 27;
	} segData;
} gameData;
struct Segment {
	struct {
		int m_nBaseTex = 0;
	} m_sides[6];
	float face_size = 1;
	float FaceSize(unsigned side) const
	{
		if (side >= 6) throw std::runtime_error("Source particle queried an invalid face");
		return face_size;
	}
} segment;
Segment *SEGMENT(int index)
{
	if (index != 0) throw std::runtime_error("Source particle queried an unexpected segment");
	return &segment;
}
bool water = false;
bool IsWaterTexture(int)
{
	return water;
}
#include "xl_reference_particle_setup.h"

#define I2X(value) ((value) * 65536)
#include "xl_reference_particle_constants.h"
#define PARTICLE_SIZE(size, scale, blow) particleManager.ParticleSize(size, scale, blow)
#define STATIC_SMOKE                     1
#define SHOW_SMOKE                       true
#define SOUNDCLASS_GENERIC               1
#define SND_ADDON_AIRBUBBLES             0
struct {
	struct {
		struct {
			int bDisperse = 1, bBubbles = 1, bStatic = 1;
		} particles;
	} render;
} options;
auto *gameOpts = &options;
struct ParticleManager {
	xl_compat::ParticleEmitterDefinition created{};
	int owner = -1, creates = 0, kills = 0, positions = 0;
#include "xl_reference_particle_sizes.h"
	int GetObjectSystem(int) const
	{
		return owner;
	}
	void SetObjectSystem(int, int value)
	{
		owner = value;
	}
	int Create(CFixVector *, CFixVector *direction, CFixMatrix *, int segment_index, int emitters, int count, float scale, int life,
	           int speed, int type, int object, CFloatVector *color, int blow, int side)
	{
		if (segment_index != 0 || emitters != 1 || object != 0 || side < -1 || side >= 6)
			throw std::runtime_error("Unexpected source particle creation binding");
		++creates;
		created = {};
		created.count = MaxParticles(count, 2);
		created.scale = scale;
		created.life = life;
		created.speed = speed;
		created.image = type;
		created.color = { color->Red(), color->Green(), color->Blue(), color->Alpha() };
		created.direction = { direction->v.coord.x, direction->v.coord.y, direction->v.coord.z };
		created.blow_up = blow != 0;
		return 7;
	}
	void SetBrightness(int, int value)
	{
		created.brightness = value;
	}
	void SetFadeType(int, int value)
	{
		created.fade = value;
	}
	void SetPos(int, CFixVector *, void *, int)
	{
		++positions;
	}
} particleManager;
void KillObjectSmoke(int)
{
	particleManager.owner = -1;
	++particleManager.kills;
}
int SRandShort()
{
	return 0;
}
int Rand(int limit)
{
	if (limit <= 0) throw std::runtime_error("Invalid emitter random bound");
	return 0;
}
const char *AddonSoundName(int)
{
	return "airbubbles.wav";
}
struct {
	void CreateObjectSound(int, int, int, int, int, int, int, int, const char *)
	{
		particleManager.created.bubbles = true;
	}
} audio;
#include "xl_reference_particle_emitter.h"

void compare_emitter(const xl_compat::ParticleEffect &effect)
{
	for (bool disperse : { false, true }) {
		const std::array<int, 3> forward{ -17031, 10000, 60000 };
		const auto actual = xl_compat::particle_emitter_definition(effect, forward, disperse);
		CObject object;
		auto &p = object.rType.particleInfo;
		p = { effect.life, { effect.size[0], effect.size[1] }, effect.particles, effect.speed, effect.drift, effect.brightness, effect.side, int(effect.kind), 1, { effect.color } };
		object.info.position.mOrient.m.dir.f = { { { forward[0], forward[1], forward[2] } } };
		gameOpts->render.particles.bDisperse = int(disperse);
		particleManager = {};
		DoStaticParticles(&object);
		const auto &source = particleManager.created;
		if (particleManager.creates != 1 || source.image != actual.image || source.fade != actual.fade || source.count != actual.count ||
		    source.life != actual.life || source.speed != actual.speed || source.scale != actual.scale || source.direction != actual.direction ||
		    source.color != actual.color || source.blow_up != actual.blow_up || source.bubbles != actual.bubbles ||
		    (!actual.bubbles && source.brightness != actual.brightness))
			throw std::runtime_error("Actual source particle emitter definition differs");
		DoStaticParticles(&object);
		if (particleManager.creates != 1) throw std::runtime_error("Source recreated a bound particle emitter");
		p.bEnabled = 0;
		DoStaticParticles(&object);
		if (particleManager.owner != -1 || particleManager.kills != 1) throw std::runtime_error("Source failed to disable a particle emitter");
	}
}

void compare(const std::vector<int> &payload, int version, float face_size, bool wet)
{
	xl_compat::Object object{};
	object.render = 11;
	object.render_data = payload;
	const auto actual = xl_compat::decode_particle_effect(object, version, face_size, wet);
	CObject source;
	auto &p = source.rType.particleInfo;
	p = { payload[0], { payload[1], 0 }, payload[2], payload[3], payload[4], payload[5], payload[10], payload[11] };
	gameData.segData.nLevelVersion = version;
	segment.face_size = face_size;
	water = wet;
	source.SetupSmoke();
	if (actual.life != p.nLife || actual.size[0] != p.nSize[0] || actual.size[1] != p.nSize[1] ||
	    actual.particles != p.nParts || actual.speed != p.nSpeed || actual.drift != p.nDrift || actual.brightness != p.nBrightness ||
	    actual.side != p.nSide || int(actual.kind) != p.nType || actual.enabled != bool(payload[12]) ||
	    actual.color != std::array<int, 4>{ payload[6], payload[7], payload[8], payload[9] })
		throw std::runtime_error("Actual source particle preparation differs");
	compare_emitter(actual);
}
} // namespace particle_reference

void particle_source_controls()
{
	static_assert(int(xl_compat::ParticleKind::smoke) == SMOKE_TYPE_SMOKE && int(xl_compat::ParticleKind::spray) == SMOKE_TYPE_SPRAY &&
	                  int(xl_compat::ParticleKind::bubbles) == SMOKE_TYPE_BUBBLES && int(xl_compat::ParticleKind::fire) == SMOKE_TYPE_FIRE &&
	                  int(xl_compat::ParticleKind::waterfall) == SMOKE_TYPE_WATERFALL && int(xl_compat::ParticleKind::rain) == SMOKE_TYPE_RAIN &&
	                  int(xl_compat::ParticleKind::snow) == SMOKE_TYPE_SNOW,
	              "Serialized particle subtype constants differ");
	unsigned count = 0;
	for (int version : { 15, 17, 18, 19, 23, 27 })
		for (int kind = 0; kind <= 6; ++kind)
			for (int side : { 0, 1, 6 })
				for (float area : { 0.0f, 0.0625f, 0.5f, 1.0f, 1.2345f, 8.0f })
					for (bool water : { false, true })
						for (int value : { 0, 1, 3, 5, 8, 10 }) {
							particle_reference::compare({ value, value, value, value, value, value, 255, 128, 17, 0, side, kind, value % 2 }, version, area, water);
							++count;
						}
	for (const auto &change : std::vector<std::pair<unsigned, int>>{ { 0, -1 }, { 0, INT32_MAX }, { 1, INT32_MAX }, { 2, 11 }, { 3, 31 }, { 4, INT32_MAX }, { 5, INT32_MAX }, { 6, 256 }, { 10, 7 }, { 11, 7 } }) {
		xl_compat::Object object{};
		object.render = 11;
		object.render_data = { 1, 1, 1, 1, 1, 1, 255, 255, 255, 255, 0, 0, 1 };
		object.render_data[change.first] = change.second;
		bool rejected = false;
		try {
			xl_compat::decode_particle_effect(object, 27);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Invalid particle arithmetic or byte field was accepted");
	}
	std::cout << "Pinned particles: " << count << " setup and " << count * 2 << " emitter comparisons, reuse/disable and invalid-arithmetic controls passed\n";
}
