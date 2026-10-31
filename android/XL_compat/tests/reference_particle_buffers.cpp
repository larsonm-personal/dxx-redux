// Execute pinned buffer decisions; replace only GPU submission with an ID receipt
#include "particle_buffer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace particle_buffer_reference
{
#define ENTER(a, b)
#define RETVAL(value)       return value;
#define USE_PARTICLE_SHADER 1
struct CFloatVector {
	std::array<float, 3> v{};
	CFloatVector operator-(const CFloatVector &other) const
	{
		return { { v[0] - other.v[0], v[1] - other.v[1], v[2] - other.v[2] } };
	}
	float Mag() const
	{
		return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}
	static float Dist(const CFloatVector &a, const CFloatVector &b)
	{
		return (a - b).Mag();
	}
	void Assign(const CFloatVector &other)
	{
		*this = other;
	}
	void operator/=(float value)
	{
		for (auto &axis : v) axis /= value;
	}
	void operator*=(float value)
	{
		for (auto &axis : v) axis *= value;
	}
	void operator+=(const CFloatVector &other)
	{
		for (unsigned i = 0; i < 3; ++i) v[i] += other.v[i];
	}
	static CFloatVector Avg(const CFloatVector &a, const CFloatVector &b)
	{
		return { { (a.v[0] + b.v[0]) / 2, (a.v[1] + b.v[1]) / 2, (a.v[2] + b.v[2]) / 2 } };
	}
};
struct CEffectArea {
	CFloatVector m_pos;
	float m_rad = 0;
	CEffectArea &Add(CFloatVector &, float);
	bool Overlap(CFloatVector &pos, float rad)
	{
		return CFloatVector::Dist(m_pos, pos) < m_rad + rad;
	}
};
struct CParticle {
	int id, type;
	CFloatVector m_vPos;
	float radius = 1;
	bool m_bEmissive = true;
	int m_iFrame = 0, m_nRotFrame = 0;
	int RenderType() const
	{
		return type;
	}
	CFloatVector &Posf()
	{
		return m_vPos;
	}
	float Rad() const
	{
		return radius;
	}
};
struct tRenderParticle {
	CParticle *particle;
	float fBrightness;
	int nFrame, nRotFrame;
};
struct CParticleBuffer : CEffectArea {
	struct Storage : std::vector<tRenderParticle> {
		Storage() : std::vector<tRenderParticle>(25000) {}
		tRenderParticle *operator+(int offset)
		{
			return data() + offset;
		}
	} m_particles;
	int m_iBuffer = 0, m_nType = -1;
	bool m_bEmissive = false;
	float m_dMax = 0;
	static int32_t bCompatible[22];
	bool Compatible(CParticle *);
	bool Add(CParticle *, float, CFloatVector &, float);
	bool Flush(float, bool = false);
	void Reset()
	{
		m_iBuffer = 0;
		m_nType = -1;
		m_bEmissive = false;
		m_dMax = 0;
		m_pos = {};
		m_rad = 0;
	}
};
struct CParticleManager {
	std::array<CParticleBuffer, 3> particleBuffer;
	bool Flush(float, bool = false);
	int16_t Add(CParticle *, float, int32_t, bool &);
	bool Add(CParticle *, float);
};
struct {
	struct {
		CFloatVector posf[1];
	} m_info;
} transformation;
struct {
	struct {
		struct {
			int nQuality = 3;
		} particles;
	} render;
} options;
auto *gameOpts = &options;
std::vector<int> drawn;
#include "xl_reference_particle_buffers.h"
bool CParticleBuffer::Flush(float, bool)
{
#include "xl_reference_particle_flush_gate.h"
	for (int i = 0; i < m_iBuffer; ++i) drawn.push_back(m_particles[std::size_t(i)].particle->id);
	Reset();
	return true;
}
} // namespace particle_buffer_reference

void particle_buffer_source_controls()
{
	using namespace particle_buffer_reference;
	static_assert(PART_BUF_SIZE == 25000 && MAX_PARTICLE_BUFFERS == 3, "Update particle buffer fixture capacity");
	unsigned cases = 0, submitted = 0;
	const auto run = [&](std::vector<CParticle> particles, const std::vector<int> &expected, int boundary = -1, bool golden = true) {
		CParticleManager source;
		xl_compat::FireParticleBuffers actual(transformation.m_info.posf[0].v);
		std::vector<int> actual_drawn;
		const auto draw = [&](const std::vector<std::size_t> &indices) {
			for (auto index : indices) actual_drawn.push_back(particles[index].id);
		};
		drawn.clear();
		for (std::size_t i = 0; i < particles.size(); ++i) {
			if (int(i) == boundary) {
				source.Flush(-1, true);
				actual.flush(draw);
			}
			source.Add(&particles[i], 1);
			const auto &p = particles[i];
			actual.add(i, p.m_vPos.v, p.radius, p.type == 16, p.m_bEmissive, draw);
			if (actual_drawn != drawn) throw std::runtime_error("Particle buffer intermediate dispatch differs from pinned source");
		}
		source.Flush(-1, true);
		actual.flush(draw);
		if (actual_drawn != drawn) throw std::runtime_error("Particle buffer final dispatch differs from pinned source");
		if (golden && drawn != expected) {
			std::string receipt;
			for (auto id : drawn) {
				if (receipt.size() > 160) {
					receipt += "...";
					break;
				}
				receipt += " " + std::to_string(id);
			}
			throw std::runtime_error("Pinned particle buffer receipt changed in profile " + std::to_string(cases) + ":" + receipt);
		}
		++cases;
		submitted += unsigned(particles.size());
	};
	const auto particle = [](int id, int type, float x) { return CParticle{ id, type, { { x, 0, 0 } }, 1, type == 5 }; };
	run({ particle(0, 5, 10) }, {});
	run({ particle(0, 5, 10), particle(1, 5, 11) }, { 0, 1 });
	run({ particle(0, 5, 10), particle(1, 16, 10), particle(2, 5, 10), particle(3, 16, 10) }, { 0, 2 });
	run({ particle(0, 5, 10), particle(1, 16, 100), particle(2, 5, 11), particle(3, 16, 101) }, { 1, 3, 0, 2 });
	run({ particle(0, 5, 10), particle(1, 5, 11) }, {}, 1);
	run({ particle(0, 5, 0), particle(1, 5, 0) }, {});
	std::vector<CParticle> capacity;
	std::vector<int> expected;
	capacity.reserve(25001);
	for (int i = 0; i < 25001; ++i) {
		capacity.push_back(particle(i, 5, 10));
		if (i < 25000) expected.push_back(i);
	}
	run(std::move(capacity), expected);
	std::uint32_t seed = 0x672f13u;
	const auto random = [&] { seed = seed * 1664525u + 1013904223u; return seed; };
	for (unsigned profile = 0; profile < 256; ++profile) {
		transformation.m_info.posf[0].v = { float(int(profile % 9) - 4), float(profile % 7), 0 };
		std::vector<CParticle> inputs;
		for (unsigned i = 0; i < 512; ++i) {
			const auto choice = random();
			auto p = particle(int(i), choice & 1 ? 5 : 16, float(int(random() % 601) - 300));
			p.m_vPos.v[1] = float(int(random() % 31) - 15);
			p.m_vPos.v[2] = float(int(random() % 31) - 15);
			p.radius = float(random() % 20) * float(profile % 4);
			// Include all four type/emissive pairs to exercise third-buffer replacement
			p.m_bEmissive = (choice & 2) != 0;
			inputs.push_back(p);
		}
		run(std::move(inputs), {}, int(profile * 2), false);
	}
	transformation.m_info.posf[0] = {};
	std::cout << "Pinned particle buffering: " << cases << " profiles, " << submitted << " inputs; every intermediate/final dispatch, singleton suppression, overlapping/disjoint variants, replacement, boundary flush, zero distance and capacity passed\n";
}
