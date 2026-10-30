#ifndef DXX_XL_PARTICLE_RENDER_H
#define DXX_XL_PARTICLE_RENDER_H
#include "particle_fire.h"
#include "particle_image.h"

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
struct ParticleVertex {
	std::array<float, 3> position{};
	std::array<float, 2> uv{};
	std::array<float, 4> color{ 1, 1, 1, 1 };
};
enum class ParticleBlend { weak_additive,
	                       multiply };
struct ParticleQuad {
	std::array<ParticleVertex, 4> vertices;
	ParticleBlend blend;
};

// The active source Setup body builds world-space vertices from fixed camera
// axes. Keep the source's operation order, atlas border and UV orientation
inline ParticleQuad fire_particle_quad(const FireParticle &particle, const ParticleView &view, unsigned image_width, unsigned image_height, bool alpha_control = false)
{
	if (particle.frame < 0 || particle.frame >= 4 || particle.orientation < 0 || particle.orientation >= 4 || particle.rotation < 0 || particle.rotation >= 64 || !image_width || !image_height)
		throw std::runtime_error("Invalid XL fire billboard state");
	ParticleQuad quad;
	quad.blend = particle.frame < 2 ? ParticleBlend::weak_additive : ParticleBlend::multiply;
	const auto &basis = particle.rotates && view.rotate ? view.rotated[std::size_t(particle.orientation & 1 ? 63 - particle.rotation : particle.rotation)] : view.basis;
	const float width = particle.width * particle.decay, height = particle.height * particle.decay;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const float center = float(particle.position[axis]) / 65536.0f;
		const float right = float(basis.right[axis]) / 65536.0f * width;
		const float up = float(basis.up[axis]) / 65536.0f * height;
		quad.vertices[0].position[axis] = (center - right) - up;
		quad.vertices[1].position[axis] = (center - right) + up;
		quad.vertices[2].position[axis] = (center + right) + up;
		quad.vertices[3].position[axis] = (center + right) - up;
	}
	const float delta = 1.0f / fire_image_atlas_side;
	const float u = float(particle.frame % int(fire_image_atlas_side)) * delta;
	const float v = float(particle.frame / int(fire_image_atlas_side)) * delta;
	const float x_border = 1.0f / float(image_width), y_border = 1.0f / float(image_height);
	for (unsigned i = 0; i < 4; ++i) {
		auto &vertex = quad.vertices[(unsigned(particle.orientation) + i) % 4];
		vertex.uv = { i == 0 || i == 3 ? u + x_border : u + delta - x_border, i < 2 ? v + y_border : v + delta - y_border };
		vertex.color[3] = alpha_control ? 0.0f : 1.0f;
	}
	return quad;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
