#ifndef DXX_XL_PARTICLE_RENDER_H
#define DXX_XL_PARTICLE_RENDER_H
#include "particle_fire.h"
#include "particle_image.h"
#include "particle_smoke.h"

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct ParticleBasis {
	ParticlePoint right{}, up{};
};
struct ParticleView {
	ParticleBasis basis;
	std::array<ParticleBasis, 64> rotated;
	bool rotate = true;
};
inline ParticleView particle_view(const ParticleBasis &basis)
{
	ParticleView result{};
	result.basis = basis;
	// Source InitRotation uses fixed bank angles and the default double math
	// profile; SetupRotation multiplies them by the viewer's fixed matrix
	constexpr double pi = 3.141592653589793240;
	for (unsigned i = 0; i < result.rotated.size(); ++i) {
		const double angle = (double(i * 1024) * 2.0 * pi) / 65536.0;
		const int sine = int(std::sin(angle) * 65536), cosine = int(std::cos(angle) * 65536);
		for (unsigned axis = 0; axis < 3; ++axis) {
			const ParticlePoint row{ basis.right[axis], basis.up[axis], 0 };
			result.rotated[i].right[axis] = fixed_vector_dot(row, { cosine, sine, 0 });
			result.rotated[i].up[axis] = fixed_vector_dot(row, { -sine, cosine, 0 });
		}
	}
	return result;
}
struct ParticleVertex {
	std::array<float, 3> position{};
	std::array<float, 2> uv{};
	std::array<float, 4> color{ 1, 1, 1, 1 };
};
enum class ParticleBlend { weak_additive,
	                       multiply,
	                       alpha };
struct ParticleQuad {
	std::array<ParticleVertex, 4> vertices;
	ParticleBlend blend;
};

inline bool prepare_fire_particle_draw(FireParticle &particle, bool animate)
{
	if (particle.life < 0) return false;
	// Source queues lazily, then advances rotation; active Setup uses the live
	// rotation member when the queued buffer is flushed
	if (animate && particle.rotates) particle.rotation = (particle.rotation + 1) % 64;
	return true;
}

inline bool prepare_smoke_particle_draw(SmokeParticle &particle, bool animate)
{
	if (particle.motion.life < 0) return false;
	if (animate) {
		particle.motion.frame = (particle.motion.frame + 1) % 64;
		if (particle.motion.rotates) {
			particle.rotation_phase <<= 1;
			if (particle.rotation_phase == 4) {
				particle.rotation_phase = 1;
				particle.motion.rotation = (particle.motion.rotation + 1) % 64;
			}
		}
	}
	return true;
}

struct ParticleAnimationClock {
	int last = 0;
	bool begin(int milliseconds, bool second_eye = false)
	{
		if (milliseconds < last) throw std::runtime_error("XL particle render clock moved backwards");
		if (milliseconds - last < 33 || second_eye) return false;
		last = milliseconds;
		return true;
	}
};

// The active source Setup body builds world-space vertices from fixed camera
// axes. Keep the source's operation order, atlas border and UV orientation
inline ParticleQuad particle_billboard_quad(const FireParticle &particle, const ParticleView &view, unsigned image_width, unsigned image_height, unsigned atlas_side, float scale, const std::array<float, 4> &color, ParticleBlend blend, bool inset = true)
{
	if (!atlas_side || particle.frame < 0 || unsigned(particle.frame) >= atlas_side * atlas_side || particle.orientation < 0 || particle.orientation >= 4 || particle.rotation < 0 || particle.rotation >= 64 || !image_width || !image_height)
		throw std::runtime_error("Invalid XL particle billboard state");
	ParticleQuad quad;
	quad.blend = blend;
	const auto &basis = particle.rotates && view.rotate ? view.rotated[std::size_t(particle.orientation & 1 ? 63 - particle.rotation : particle.rotation)] : view.basis;
	const float width = particle.width * scale, height = particle.height * scale;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const float center = float(particle.position[axis]) / 65536.0f;
		const float right = float(basis.right[axis]) / 65536.0f * width;
		const float up = float(basis.up[axis]) / 65536.0f * height;
		quad.vertices[0].position[axis] = (center - right) - up;
		quad.vertices[1].position[axis] = (center - right) + up;
		quad.vertices[2].position[axis] = (center + right) + up;
		quad.vertices[3].position[axis] = (center + right) - up;
	}
	const float delta = 1.0f / float(atlas_side);
	const float u = float(particle.frame % int(atlas_side)) * delta;
	const float v = float(particle.frame / int(atlas_side)) * delta;
	const float x_border = inset ? 1.0f / float(image_width) : 0, y_border = inset ? 1.0f / float(image_height) : 0;
	for (unsigned i = 0; i < 4; ++i) {
		auto &vertex = quad.vertices[(unsigned(particle.orientation) + i) % 4];
		vertex.uv = { i == 0 || i == 3 ? u + x_border : u + delta - x_border, i < 2 ? v + y_border : v + delta - y_border };
		vertex.color = color;
	}
	return quad;
}

inline ParticleQuad fire_particle_quad(const FireParticle &particle, const ParticleView &view, unsigned image_width, unsigned image_height, bool alpha_control = false)
{
	return particle_billboard_quad(particle, view, image_width, image_height, fire_image_atlas_side, particle.decay, { 1, 1, 1, alpha_control ? 0.0f : 1.0f }, particle.frame < 2 ? ParticleBlend::weak_additive : ParticleBlend::multiply);
}

inline ParticleQuad smoke_particle_quad(const SmokeParticle &particle, const ParticleView &view, unsigned image_width, unsigned image_height)
{
	const float decay = particle.motion.decay;
	if (!(decay > 0) || decay > 1) throw std::runtime_error("XL smoke draw requires positive remaining lifetime");
	float scale = particle.blow_up ? 1.0f / float(std::sqrt(decay)) : 1.0f;
	if (decay > 0.9f) scale *= std::sqrt((1.0f - decay) / 0.1f);
	return particle_billboard_quad(particle.motion, view, image_width, image_height, smoke_image_atlas_side, scale, particle.appearance.rendered, ParticleBlend::alpha, false);
}

inline void particle_camera_quad(ParticleQuad &quad, const ParticlePoint &eye, const std::array<ParticlePoint, 3> &view)
{
	for (auto &vertex : quad.vertices) {
		std::array<float, 3> delta;
		for (unsigned axis = 0; axis < 3; ++axis) delta[axis] = vertex.position[axis] - float(eye[axis]) / 65536.0f;
		for (unsigned row = 0; row < 3; ++row) {
			vertex.position[row] = (delta[0] * (float(view[row][0]) / 65536.0f) + delta[1] * (float(view[row][1]) / 65536.0f)) + delta[2] * (float(view[row][2]) / 65536.0f);
		}
		vertex.position[2] = -vertex.position[2];
	}
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
