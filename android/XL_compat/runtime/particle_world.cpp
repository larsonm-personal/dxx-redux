#include "particle_world.h"
#include "geometry.h"
extern "C" {
#include "gameseg.h"
}
#undef min
#undef max

namespace xl_compat
{
ParticleSide compile_particle_side(int segment, int side, const ClassicLightingSide &geometry)
{
	const auto &native = Segments[segment];
	ParticleSide compiled;
	compiled.child = native.children[side];
	if (geometry.face.corner_count < 3) return compiled;
	compiled.normals = geometry.fixed_normals;
	int vertices[4];
	get_side_verts(vertices, segment, side);
	if (geometry.face.corner_count == 3) {
		compiled.faces = 1;
		const auto &anchor = Vertices[(std::min) ({ vertices[0], vertices[1], vertices[2] })];
		compiled.anchor = { anchor.x, anchor.y, anchor.z };
		return compiled;
	}
	compiled.faces = 2;
	std::array<int, 6> triangles;
	for (unsigned face = 0; face < 2; ++face)
		for (unsigned corner = 0; corner < 3; ++corner) triangles[face * 3 + corner] = vertices[geometry.face.triangles[face][corner]];
	const int anchor = std::min(triangles[0], triangles[2]), opposite = std::min(triangles[1], triangles[4]);
	const auto &a = Vertices[anchor], &b = Vertices[opposite];
	compiled.anchor = { a.x, a.y, a.z };
	compiled.pokes_out = particle_plane_distance({ b.x, b.y, b.z }, compiled, unsigned(triangles[4] >= triangles[1])) > ParticleWorld::plane_tolerance;
	return compiled;
}

ParticleWorld compile_particle_world(const Inventory &source)
{
	if (Highest_segment_index < 0 || source.segments.size() != std::size_t(Highest_segment_index) + 1 || source.vertices.size() != std::size_t(Highest_vertex_index) + 1)
		throw std::runtime_error("XL particle world does not match the loaded mine");
	ParticleWorld result;
	result.segments.resize(source.segments.size());
	for (std::size_t i = 0; i < source.vertices.size(); ++i) {
		const auto &native = Vertices[i];
		if (source.vertices[i] != ParticlePoint{ native.x, native.y, native.z })
			throw std::runtime_error("XL particle world vertices do not match the loaded mine");
	}
	for (std::size_t index = 0; index < result.segments.size(); ++index) {
		const auto &native = Segments[index];
		for (unsigned vertex = 0; vertex < 8; ++vertex)
			if (source.segments[index].vertices[vertex] < 0xfff8 && source.segments[index].vertices[vertex] != native.verts[vertex])
				throw std::runtime_error("XL particle world connectivity does not match the loaded mine");
		auto &segment = result.segments[index];
		segment.bounds = { ParticlePoint{ INT32_MAX, INT32_MAX, INT32_MAX }, ParticlePoint{ INT32_MIN, INT32_MIN, INT32_MIN } };
		segment.skybox = segment_is_skybox(source, source.segments[index]);
		segment.water = (source.segments[index].properties & segment_water) != 0;
		segment.lava = (source.segments[index].properties & segment_lava) != 0;
		segment.inner_radius = INT32_MAX;
		for (unsigned side = 0; side < 6; ++side) {
			if (source.segments[index].children[side] != native.children[side])
				throw std::runtime_error("XL particle world portals do not match the loaded mine");
			const auto geometry = compile_classic_lighting_side(int(index), int(side));
			segment.center = geometry.segment_center;
			auto &compiled = segment.sides[side];
			compiled = compile_particle_side(int(index), int(side), geometry);
			if (!compiled.faces) continue;
			std::int64_t distance = 0;
			unsigned inside = 0;
			for (unsigned face = 0; face < compiled.faces; ++face) {
				const int projection = particle_plane_distance(segment.center, compiled, face);
				if (projection >= -ParticleWorld::plane_tolerance) {
					distance += projection;
					++inside;
				}
			}
			if (inside == 2) distance /= 2;
			segment.inner_radius = std::min(segment.inner_radius, particle_detail::signed_checked(distance));
		}
		for (const auto id : source.segments[index].vertices) {
			if (id >= 0xfff8) continue;
			if (id < 0 || id > Highest_vertex_index) throw std::runtime_error("Invalid XL particle-world vertex");
			const auto &v = Vertices[id];
			const ParticlePoint vertex{ v.x, v.y, v.z };
			for (unsigned axis = 0; axis < 3; ++axis) {
				segment.bounds[0][axis] = std::min(segment.bounds[0][axis], vertex[axis]);
				segment.bounds[1][axis] = std::max(segment.bounds[1][axis], vertex[axis]);
			}
			segment.outer_radius = std::max(segment.outer_radius, geometry_default_magnitude(particle_difference({ v.x, v.y, v.z }, segment.center)));
		}
	}
	result.build_grids();
	return result;
}
} // namespace xl_compat
