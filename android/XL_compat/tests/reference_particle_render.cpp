// Camera axes are explicit inputs; the pinned active Setup body builds vertices
#include "particle_render.h"
#include <cmath>
#include <iostream>
namespace particle_render_reference
{
#include "xl_reference_particle_constants.h"
constexpr double PI = 3.141592653589793240;
using fix = std::int32_t;
#include "xl_reference_particle_math.h"
const std::int16_t sincos_table[321]{};
struct CFixVector {
	union {
		std::array<int, 3> values{};
		struct {
			struct {
				int x, y, z;
			} coord;
		} v;
	};
	static const fix Dot(const CFixVector &, const CFixVector &);
};
struct CAngleVector {
	struct {
		struct {
			int p, b, h;
		} coord;
	} v{};
	void SetZero()
	{
		v = {};
	}
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u, f;
		} dir;
	} m;
	static const CFixMatrix Create(fix, fix, fix, fix, fix, fix);
	static const CFixMatrix Create(const CAngleVector &);
	CFixMatrix Mul(const CFixMatrix &);
	CFixMatrix operator*(const CFixMatrix &other)
	{
		return Mul(other);
	}
};
struct CFloatVector3 {
	union {
		struct {
			float x, y, z;
		} coord;
		float values[3];
	} v{};
	void Assign(const CFixVector &p)
	{
		for (unsigned i = 0; i < 3; ++i) v.values[i] = float(p.values[i]) / 65536.0f;
	}
	CFloatVector3 &operator*=(float scale)
	{
		for (auto &value : v.values) value *= scale;
		return *this;
	}
	CFloatVector3 &operator+=(const CFloatVector3 &p)
	{
		for (unsigned i = 0; i < 3; ++i) v.values[i] += p.v.values[i];
		return *this;
	}
	CFloatVector3 &operator-=(const CFloatVector3 &p)
	{
		for (unsigned i = 0; i < 3; ++i) v.values[i] -= p.v.values[i];
		return *this;
	}
	CFloatVector3 operator-(const CFloatVector3 &p) const
	{
		auto r = *this;
		return r -= p;
	}
	CFloatVector3 operator+(const CFloatVector3 &p) const
	{
		auto r = *this;
		return r += p;
	}
	void Neg()
	{
		*this *= -1;
	}
	static void Normalize(CFloatVector3 &)
	{
		throw std::runtime_error("Unexpected non-fire normalization");
	}
	static CFloatVector3 Cross(const CFloatVector3 &, const CFloatVector3 &)
	{
		throw std::runtime_error("Unexpected non-fire cross product");
	}
};
struct Color {
	std::array<float, 4> values{ 1, 1, 1, 1 };
	float &Alpha()
	{
		return values[3];
	}
};
struct TexCoord {
	struct {
		float u = 0, v = 0, l = 0;
	} v;
};
struct tParticleVertex {
	CFloatVector3 vertex;
	Color color;
	TexCoord texCoord;
};
struct {
	struct {
		struct {
			struct {
				CFixMatrix mOrient;
				CFixVector vPos;
			} viewer;
		} mine;
	} renderData;
} gameData;
struct {
	struct {
		int nMathFormat = 2;
		struct {
			bool bWiggleBubbles = false, bWobbleBubbles = false, bRotate = true;
		} particles;
	} render;
} options;
auto *gameOpts = &options;
struct ImageInfo {
	float xBorder = 0, yBorder = 0;
} image_info;
ImageInfo &ParticleImageInfo(int type)
{
	if (type != FIRE_PARTICLES && type != SMOKE_PARTICLES) throw std::runtime_error("Unexpected particle image");
	return image_info;
}
int Rand(int)
{
	throw std::runtime_error("Fire Setup consumed random input");
}
struct CParticle;
struct CParticleManager {
	bool m_bAnimate = false;
	unsigned queued = 0;
	bool Add(CParticle *, float)
	{
		++queued;
		return false;
	}
	bool Animate() const
	{
		return m_bAnimate;
	}
	void SetLastType(int) {}
	int32_t BeginRender(int32_t, float);
} particleManager;
struct {
	struct {
		int nSDLTicks[1]{};
	} app;
} gameStates;
struct {
	int separation = 0;
	int StereoSeparation() const
	{
		return separation;
	}
} ogl;
struct CParticle {
	CFixVector m_vPos, m_vDir;
	CFixMatrix mSparkOrient;
	static CFixMatrix mRot[2][64];
	int m_nType = FIRE_PARTICLES, m_nFrames = 2, m_iFrame = 0, m_nFadeType = 3, m_bBlowUp = 0, m_bEmissive = 2, m_nOrient = 0, m_nRotFrame = 0, m_bRotate = 0;
	int m_nDelay = 0, m_nLife = 1, m_bAnimate = 0;
	float m_decay = 0, m_nHeight = 0, m_nWidth = 0, m_deltaUV = .5f;
	Color m_renderColor;
	TexCoord m_texCoord;
	int32_t RenderType();
	int32_t Render(float);
	void UpdateTexCoord();
	void Setup(bool, float, char, char, tParticleVertex *, int32_t);
	static void InitRotation();
	static void SetupRotation();
};
CFixMatrix CParticle::mRot[2][64]{};
#include "xl_reference_particle_camera.h"
#include "xl_reference_particle_clock.h"
#define ENABLE_RENDER     1
#define LAZY_RENDER_SETUP 1
#define PROF_START
#define PROF_END(a)
#include "xl_reference_particle_render.h"
} // namespace particle_render_reference

void particle_smoke_render_controls()
{
	using namespace particle_render_reference;
	const xl_compat::ParticleBasis basis{ { 60491, 9001, -23411 }, { -12017, 64003, 3501 } };
	gameData.renderData.mine.viewer.mOrient.m.dir.r.values = basis.right;
	gameData.renderData.mine.viewer.mOrient.m.dir.u.values = basis.up;
	CParticle::InitRotation();
	CParticle::SetupRotation();
	auto view = xl_compat::particle_view(basis);
	unsigned count = 0;
	for (int frame : { 0, 7, 8, 31, 63 })
		for (int orientation = 0; orientation < 4; ++orientation)
			for (int rotation : { 0, 17, 63 })
				for (bool blow_up : { false, true })
					for (bool rotate : { false, true })
						for (float decay : { 0.001f, 0.125f, 0.75f, 0.9f, 0.901f, 1.0f }) {
							xl_compat::SmokeParticle particle;
							auto &motion = particle.motion;
							motion.position = { -281117, 723003, 1759911 };
							motion.frame = frame;
							motion.orientation = orientation;
							motion.rotation = rotation;
							motion.width = 1.3125f;
							motion.height = 2.875f;
							motion.decay = decay;
							particle.blow_up = blow_up;
							particle.appearance.rendered = { 0.2f, 0.4f, 0.7f, 0.13f };
							view.rotate = rotate;
							gameOpts->render.particles.bRotate = rotate;
							CParticle source;
							source.m_nType = SMOKE_PARTICLES;
							source.m_nFadeType = 1;
							source.m_bEmissive = 0;
							source.m_bBlowUp = blow_up;
							source.m_vPos.values = motion.position;
							source.m_nOrient = orientation;
							source.m_nRotFrame = rotation;
							source.m_bRotate = 1;
							source.m_nWidth = motion.width;
							source.m_nHeight = motion.height;
							source.m_decay = decay;
							source.m_deltaUV = 0.125f;
							source.m_texCoord.v.u = float(frame % 8) / 8;
							source.m_texCoord.v.v = float(frame / 8) / 8;
							source.m_renderColor.values = particle.appearance.rendered;
							image_info = { 0, 0 };
							tParticleVertex expected[4];
							source.Setup(true, .13f, char(frame), char(rotation), expected, 0);
							const auto actual = xl_compat::smoke_particle_quad(particle, view, 512, 256);
							for (unsigned i = 0; i < 4; ++i) {
								for (unsigned axis = 0; axis < 3; ++axis)
									if (actual.vertices[i].position[axis] != expected[i].vertex.v.values[axis]) throw std::runtime_error("Smoke billboard vertex differs");
								if (actual.vertices[i].color != expected[i].color.values || actual.vertices[i].uv[0] != expected[i].texCoord.v.u || actual.vertices[i].uv[1] != expected[i].texCoord.v.v) throw std::runtime_error("Smoke billboard color or UV differs");
							}
							++count;
						}
	std::cout << "Pinned smoke billboards: " << count << " geometry, rotation, decay, RGBA and atlas comparisons passed\n";
	count = 0;
	for (bool animate : { false, true })
		for (bool rotate : { false, true })
			for (int life : { -1, 0, 100 }) {
				xl_compat::SmokeParticle actual;
				actual.motion.life = life;
				actual.motion.frame = 61;
				actual.motion.rotation = 62;
				actual.motion.rotates = rotate;
				CParticle source;
				source.m_nType = SMOKE_PARTICLES;
				source.m_nLife = life;
				source.m_nFrames = 8;
				source.m_deltaUV = 0.125f;
				source.m_iFrame = 61;
				source.m_nRotFrame = 62;
				source.m_bRotate = rotate;
				source.m_bAnimate = true;
				source.UpdateTexCoord();
				particleManager.m_bAnimate = animate;
				for (unsigned i = 0; i < 140; ++i) {
					const auto queued = particleManager.queued;
					source.Render(1);
					const bool draw = xl_compat::prepare_smoke_particle_draw(actual, animate);
					if (draw != (particleManager.queued > queued) || actual.motion.frame != source.m_iFrame || actual.motion.rotation != source.m_nRotFrame || (rotate && actual.rotation_phase != source.m_bRotate) || source.m_texCoord.v.u != float(actual.motion.frame % 8) / 8 || source.m_texCoord.v.v != float(actual.motion.frame / 8) / 8)
						throw std::runtime_error("Smoke lazy-render atlas or rotation cadence differs");
					++count;
				}
			}
	std::cout << "Pinned smoke animation: " << count << " queued atlas and half-rate rotation transitions passed\n";
}

void particle_render_source_controls()
{
	using namespace particle_render_reference;
	xl_compat::ParticleAnimationClock clock;
	unsigned clock_cases = 0;
	for (int time : { 0, 1, 32, 33, 34, 65, 66, 99, 100, 110, 333, 1000 })
		for (bool second_eye : { true, false }) {
			gameStates.app.nSDLTicks[0] = time;
			ogl.separation = second_eye ? -1 : 0;
			particleManager.BeginRender(-1, 1);
			if (clock.begin(time, second_eye) != particleManager.Animate()) throw std::runtime_error("Particle animation gate differs from source");
			++clock_cases;
		}
	unsigned render_cases = 0;
	for (bool animate : { false, true })
		for (bool rotates : { false, true })
			for (int life : { -1, 0, 100 }) {
				CParticle source{};
				source.m_nLife = life;
				source.m_nRotFrame = 61;
				source.m_bRotate = rotates ? -1 : 0;
				particleManager.m_bAnimate = animate;
				particleManager.queued = 0;
				xl_compat::FireParticle actual{};
				actual.life = life;
				actual.rotation = 61;
				actual.rotates = rotates;
				for (unsigned i = 0; i < 70; ++i) {
					const auto before = particleManager.queued;
					source.Render(1);
					const bool draw = xl_compat::prepare_fire_particle_draw(actual, animate);
					if (draw != (particleManager.queued > before) || actual.rotation != source.m_nRotFrame)
						throw std::runtime_error("Fire render lifetime/rotation differs from source");
					++render_cases;
				}
			}
	unsigned camera_cases = 0;
	for (int heading : { 0, 1024, 10001, 16384, 32767, -20001 })
		for (int pitch : { 0, 3127, -6213 }) {
			CAngleVector angle;
			angle.v.coord = { pitch, heading / 3, heading };
			auto &camera = gameData.renderData.mine.viewer.mOrient;
			camera = CFixMatrix::Create(angle);
			const auto actual = xl_compat::particle_view({ camera.m.dir.r.values, camera.m.dir.u.values });
			CParticle source{};
			source.InitRotation();
			source.SetupRotation();
			for (unsigned i = 0; i < 64; ++i) {
				if (actual.rotated[i].right != source.mRot[1][i].m.dir.r.values || actual.rotated[i].up != source.mRot[1][i].m.dir.u.values)
					throw std::runtime_error("Particle camera rotation differs from source");
				++camera_cases;
			}
		}
	xl_compat::ParticleView view{};
	view.basis = { { 60491, 9001, -23411 }, { -12017, 64003, 3501 } };
	gameData.renderData.mine.viewer.mOrient.m.dir.r.values = view.basis.right;
	gameData.renderData.mine.viewer.mOrient.m.dir.u.values = view.basis.up;
	CParticle source{};
	// Distinct supplied axes expose wrong rotation selection independently of the
	// separate camera/matrix consumer; these need not be orthonormal
	for (unsigned i = 0; i < 64; ++i) {
		view.rotated[i] = { { int(50001 + i * 173), int(21003 - i * 91), -13011 }, { -19007, int(48003 + i * 57), int(14003 + i * 83) } };
		source.mRot[1][i].m.dir.r.values = view.rotated[i].right;
		source.mRot[1][i].m.dir.u.values = view.rotated[i].up;
	}
	unsigned cases = 0;
	for (int frame : { 0, 2 })
		for (int orientation = 0; orientation < 4; ++orientation)
			for (int rotation : { 0, 1, 31, 63 })
				for (unsigned policy = 0; policy < 4; ++policy)
					for (unsigned extent : { 8u, 128u, 512u })
						for (float decay : { 0.0f, .125f, .75f, 1.0f }) {
							xl_compat::FireParticle particle{};
							particle.position = { -281117, 723003, 1759911 };
							particle.frame = frame;
							particle.orientation = orientation;
							particle.rotation = rotation;
							particle.rotates = (policy & 1) != 0;
							particle.width = 1.3125f;
							particle.height = 2.875f;
							particle.decay = decay;
							view.rotate = (policy & 2) != 0;
							const bool alpha = (cases & 1) != 0;
							source.m_vPos.values = particle.position;
							source.m_iFrame = frame;
							source.m_nOrient = orientation;
							source.m_nRotFrame = rotation;
							source.m_bRotate = particle.rotates ? -1 : 0;
							source.m_nWidth = particle.width;
							source.m_nHeight = particle.height;
							source.m_decay = decay;
							source.m_texCoord.v.u = float(frame % 2) * .5f;
							source.m_texCoord.v.v = float(frame / 2) * .5f;
							gameOpts->render.particles.bRotate = view.rotate;
							image_info = { 1.0f / float(extent), 1.0f / float(extent) };
							tParticleVertex expected[4];
							source.Setup(alpha, .13f, char(frame), char(rotation), expected, 0);
							const auto actual = xl_compat::fire_particle_quad(particle, view, extent, extent, alpha);
							if ((actual.blend == xl_compat::ParticleBlend::multiply) != (source.RenderType() >= PARTICLE_TYPES)) throw std::runtime_error("Fire billboard blend selection differs from source");
							for (unsigned i = 0; i < 4; ++i) {
								const auto &a = actual.vertices[i];
								const auto &b = expected[i];
								for (unsigned axis = 0; axis < 3; ++axis)
									if (a.position[axis] != b.vertex.v.values[axis]) throw std::runtime_error("Fire billboard world vertex differs from source");
								if (a.color != b.color.values || a.uv[0] != b.texCoord.v.u || a.uv[1] != b.texCoord.v.v) throw std::runtime_error("Fire billboard color/atlas coordinate differs from source");
							}
							++cases;
						}
	std::cout << "Pinned fire billboards: " << cases << " world-space quad, UV border/orientation, decay, rotation policy, white color and render-type comparisons; " << camera_cases << " source-built camera rotations, " << render_cases << " lazy-render animation/lifetime transitions and " << clock_cases << " animation tick gates passed\n";
}
