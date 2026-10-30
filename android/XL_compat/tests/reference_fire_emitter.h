// Included after the shared fire adapter; reuse actual particle creation/update
namespace particle_geometry_reference
{
static_assert(xl_compat::particle_system_limit == MAX_PARTICLE_SYSTEMS, "Particle system pool capacity differs from pinned source");
template <typename T>
struct EmitterArray {
	std::vector<T> data;
	bool Create(int size)
	{
		// Fresh pinned CArray has length zero and a null buffer; Create(0) returns null
		data.resize(std::size_t(size));
		return !data.empty();
	}
	void Clear()
	{
		std::fill(data.begin(), data.end(), T{});
	}
	operator bool() const
	{
		return !data.empty();
	}
	T &operator[](int index)
	{
		return data.at(std::size_t(index));
	}
};
struct CObject {
	struct {
		int nType = 20, nId = 0;
	} info;
} emitter_object;
CObject *OBJECT(int)
{
	return &emitter_object;
}
int nPartSeg[1]{};
struct CParticleEmitter {
	EmitterArray<CParticle> m_particles;
	int m_nLife = 0, m_nBirth = 0, m_nSpeed = 0, m_nType = 0, m_nFadeType = 0, m_nClass = 0;
	int m_nParts = 0, m_nMoved = 0, m_nPartLimit = 0, m_nMaxParts = 0, m_nFirstPart = 0, m_nSegment = 0, m_nObject = 0, m_nObjType = 0, m_nObjId = 0;
	int m_nTicks = 0, m_nDefBrightness = 0, m_nPartsPerPos = 0;
	float m_fScale = 0, m_fPartsPerTick = 0, m_fBrightness = 0;
	bool m_bHaveColor = false, m_bHaveDir = false, m_bHavePrevPos = false, m_bBlowUpParts = false, m_bEmittingFace = false;
	CFloatVector m_color;
	CFixVector m_vDir, m_vPrevPos, m_vPos, m_vEmittingFace[4];
	CFixMatrix m_mOrient;
	// Authored effects use class zero; fire's brightness branch returns one
	char ObjectClass(int)
	{
		return 0;
	}
	float Brightness()
	{
		if (m_nType != 5) throw std::runtime_error("Emitter adapter requires fire brightness");
		return 1;
	}
	int32_t Create(CFixVector *, CFixVector *, CFixMatrix *, int16_t, int32_t, int32_t, float, int32_t, int32_t, char, CFloatVector *, int32_t, int32_t, CFixVector *);
	int32_t Update(int32_t, int32_t);
	void SetLife(int32_t);
	bool IsAlive(int32_t);
};
#include "xl_reference_fire_emitter.h"

void compare_emitter(const xl_compat::FireEmitter &actual, CParticleEmitter &source, const RandomInput &random, const WorldInput &world)
{
	if (actual.count != source.m_nParts || actual.first != source.m_nFirstPart || actual.ticks != source.m_nTicks || actual.moved != source.m_nMoved || actual.life != source.m_nLife || actual.rate != source.m_fPartsPerTick || actual.demand != source.m_nPartsPerPos || actual.previous != point(source.m_vPrevPos))
		throw std::runtime_error("Pinned authored fire emitter scheduling differs");
	if (random.state != source_random.state || random.trace != source_random.trace || world.lookups != source_world.lookups || world.collisions != source_world.collisions)
		throw std::runtime_error("Pinned authored fire emitter random/world call order differs");
	for (int i = 0; i < actual.count; ++i) {
		const int slot = (actual.first + i) % int(actual.slots.size());
		const auto &a = actual.slots[std::size_t(slot)];
		const auto &b = source.m_particles[slot];
		if (a.position != point(b.m_vPos) || a.start != point(b.m_vStartPos) || a.drift != point(b.m_vDrift) || a.direction != point(b.m_vDir) || a.life != b.m_nLife || a.ttl != b.m_nTTL || a.updated != b.m_nUpdated || a.moved != b.m_nMoved || a.segment != b.m_nSegment || a.frame != b.m_iFrame || a.rotation != b.m_nRotFrame || a.orientation != b.m_nOrient || a.bounce != b.m_nBounce || a.locate_delay != b.m_nDelayPosUpdate || a.radius != b.m_nRad || a.width != b.m_nWidth || a.height != b.m_nHeight || a.decay != b.m_decay)
			throw std::runtime_error("Pinned authored fire emitter particle ring differs");
	}
}
} // namespace particle_geometry_reference

static void particle_emitter_controls(const xl_compat::Inventory *level, const xl_compat::ParticleWorld *compiled)
{
	using namespace particle_geometry_reference;
	unsigned updates = 0, drained = 0, empty = 0, lookups = 0, collisions = 0;
	const xl_compat::ParticleFace supplied_face{ { { { -655360, -655360, 0 }, { 655360, -655360, 0 }, { 655360, 655360, 0 }, { -655360, 655360, 0 } } }, 4 };
	for (int capacity : { 0, 1, 2, 7, 64 })
		for (bool surface : { false, true })
			for (unsigned seed : { 1u, 17u, 12345u }) {
				// Ring capacities and allocation rejection are covered by the supplied
				// world matrix; loaded geometry needs only one nontrivial ring size
				if (compiled && capacity != 7) continue;
				const int segment = compiled ? int(seed % compiled->segments.size()) : 0;
				const auto face = level ? xl_compat::particle_face(*level, segment, int(seed % 6), false) : supplied_face;
				xl_compat::Object authored{};
				authored.render = 11;
				authored.render_data = { 1, 1, 1, 1, 1, 10, 255, 128, 0, 128, surface ? 1 : 0, 3, 1 };
				auto definition = xl_compat::particle_emitter_definition(xl_compat::decode_particle_effect(authored, 27), { 0, 0, 65536 }, true);
				definition.count = capacity;
				const xl_compat::ParticlePoint position = compiled ? compiled->segments[segment].center : xl_compat::ParticlePoint{ 112233, -445566, 778899 };
				const bool skybox = compiled && compiled->segments[segment].skybox;
				particle_segment.function = skybox ? 10 : 0;
				CParticleEmitter source;
				auto origin = point(position), direction = point(definition.direction);
				CFixMatrix orientation;
				CFloatVector color;
				for (unsigned i = 0; i < 4; ++i) color.v.vec[i] = definition.color[i];
				CFixVector corners[4];
				for (unsigned i = 0; i < 4; ++i) corners[i] = point(face.corners[i]);
				const bool created = source.Create(&origin, &direction, &orientation, int16_t(segment), 17, capacity, definition.scale, definition.life, definition.speed, 5, &color, 123, 0, surface ? corners : nullptr) != 0;
				if (!capacity) {
					bool rejected = false;
					try {
						xl_compat::FireEmitter invalid(definition, position, surface ? &face : nullptr, segment, skybox, 123);
					} catch (const std::runtime_error &) {
						rejected = true;
					}
					if (created || !rejected) throw std::runtime_error("Zero-capacity fire emitter was created");
					++empty;
					continue;
				}
				if (!created) throw std::runtime_error("Source fire emitter allocation failed");
				xl_compat::FireEmitter actual(definition, position, surface ? &face : nullptr, segment, skybox, 123);
				source.m_nFadeType = 3;
				RandomInput random;
				random.generated = true;
				random.native_range = compiled != nullptr;
				random.state = seed;
				source_random = random;
				WorldInput world;
				world.compiled = compiled;
				source_world = world;
				gameOpts->render.particles.bCollisions = 1;
				int time = 123;
				for (int step = 0; step < 120; ++step) {
					const int intervals[] = { 0, 1, 16, 33, 249, 250, 251, 1000 };
					time += intervals[step % 8];
					actual.position[0] += step * 17 - 500;
					source.m_vPos = point(actual.position);
					if (step == 30 || step == 70) {
						actual.set_enabled(false);
						source.SetLife(0);
					}
					if (step == 45) {
						actual.set_enabled(true);
						source.SetLife(definition.life);
					}
					if (step == 12) {
						world.normal = xl_compat::ParticlePoint{ 65536, 0, 0 };
						source_world.normal = world.normal;
					}
					if (step == 18) {
						world.normal.reset();
						source_world.normal.reset();
					}
					if (step == 24 || step == 28) {
						world.located = step == 24 ? -1 : 0;
						source_world.located = world.located;
					}
					if (actual.update(time, random, world, true) != source.Update(time, 0)) throw std::runtime_error("Source fire emitter count differs");
					compare_emitter(actual, source, random, world);
					++updates;
				}
				if (!actual.drained()) throw std::runtime_error("Disabled fire emitter did not drain");
				lookups += unsigned(world.lookups);
				collisions += unsigned(world.collisions);
				++drained;
			}
	if (compiled && (!lookups || !collisions)) throw std::runtime_error("Loaded emitter controls did not exercise world consumers");
	std::cout << (compiled ? "Pinned loaded-world fire emitter: " : "Pinned fire emitter: ") << updates << " frame comparisons, " << drained << " drained rings, " << empty << " zero-capacity rejections, " << lookups << " lookups, " << collisions << " collisions, irregular timing, movement, disable/re-enable and full particle/RNG state passed\n";
}

void particle_emitter_source_controls()
{
	particle_emitter_controls(nullptr, nullptr);
}

void particle_emitter_loaded_controls(const xl_compat::Inventory &level, const xl_compat::ParticleWorld &world)
{
	// Source geometry was independently compiled from this inventory by the caller
	particle_emitter_controls(&level, &world);
}
