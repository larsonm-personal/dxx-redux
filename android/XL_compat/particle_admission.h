#ifndef DXX_XL_PARTICLE_ADMISSION_H
#define DXX_XL_PARTICLE_ADMISSION_H
#include "particle_effect.h"
#include "particle_geometry.h"
#include <optional>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
inline bool authored_particle_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 0 && object.render == 11 && object.reader_render == 11 &&
	       !object.control && !object.movement && !object.multiplayer && object.render_data.size() == 13 &&
	       object.render_data[11] >= 0 && object.render_data[11] <= 6;
}
inline bool fire_particle_shape(const Object &object)
{
	return authored_particle_shape(object) && object.render_data[11] == 3;
}
struct PreparedFireParticle {
	ParticleEffect effect;
	ParticleEmitterDefinition definition;
	std::optional<ParticleFace> face;
};
inline PreparedFireParticle prepare_authored_particle(const Inventory &level, const Object &object)
{
	if (!authored_particle_shape(object)) {
		throw std::runtime_error("XL particles require a stationary uncontrolled single-player particle object with a supported subtype");
	}
	if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size())
		throw std::runtime_error("XL particle object references an invalid segment");
	PreparedFireParticle result;
	float area = 1;
	if (object.render_data[10] > 0) {
		area = particle_face_size(particle_face(level, object.segment, object.render_data[10] - 1, true));
		result.face = particle_face(level, object.segment, object.render_data[10] - 1, false);
		if (result.face->count != 4) throw std::runtime_error("XL particle triangular emission faces remain unqualified");
	}
	result.effect = decode_particle_effect(object, level.level_version, area);
	result.definition = particle_emitter_definition(result.effect, { object.orientation[6], object.orientation[7], object.orientation[8] }, true);
	if ((result.effect.kind == ParticleKind::bubbles || result.effect.kind >= ParticleKind::waterfall) &&
	    (result.definition.count <= 0 || result.definition.life >= 0 || result.definition.life == INT32_MIN ||
	     result.definition.speed < 0 || result.definition.speed > INT32_MAX / 65536 ||
	     !(result.definition.scale < 0) || !std::isfinite(result.definition.scale) ||
	     double(-result.definition.scale) > INT32_MAX))
		throw std::runtime_error("XL authored particle emitter exceeds the bounded native creation range");
	return result;
}
inline PreparedFireParticle prepare_fire_particle(const Inventory &level, const Object &object)
{
	if (!fire_particle_shape(object)) throw std::runtime_error("XL fire requires a stationary uncontrolled single-player particle object");
	return prepare_authored_particle(level, object);
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
