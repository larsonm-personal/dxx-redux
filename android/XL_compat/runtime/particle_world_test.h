#include "particle_world.h"
#include "../tests/particle_world_receipt.h"

static void particle_world_native_case(std::ostream &out, const char *filename, unsigned ordinal)
{
	const auto native_state = [] {
		return std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments) +
		       std::string(reinterpret_cast<const char *>(Vertices), sizeof(*Vertices) * Num_vertices) +
		       std::string(reinterpret_cast<const char *>(Objects), sizeof(*Objects) * (Highest_object_index + 1)) +
		       std::string(reinterpret_cast<const char *>(Light_subtracted), sizeof(*Light_subtracted) * Num_segments) +
		       Gamesave_current_filename;
	};
	const auto before = native_state();
	const auto simulation_calls = d_rand_get_stream_call_count(D_RNG_SIM), effects_calls = d_rand_get_stream_call_count(D_RNG_FX);
	PHYSFS_File *file = PHYSFS_openRead(filename);
	require(file && PHYSFS_fileLength(file) > 0 && PHYSFS_fileLength(file) <= 64 * 1024 * 1024, "Particle world source missing or oversized");
	xl_compat::Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
	const auto consumed = PHYSFS_readBytes(file, bytes.data(), bytes.size());
	PHYSFS_close(file);
	require(consumed == PHYSFS_sint64(bytes.size()), "Particle world source truncated");
	xl_particle_world_receipt::Case receipt;
	if (ordinal) receipt.source = xl_compat::inspect_level(bytes);
	else {
		// The XL inspector deliberately excludes stock version 6; supply only loaded
		// vertices/connectivity, never Redux's computed normals, centers or radii
		receipt.source.level_version = 6;
		for (int i = 0; i < Num_vertices; ++i) receipt.source.vertices.push_back({ Vertices[i].x, Vertices[i].y, Vertices[i].z });
		receipt.source.segments.resize(Num_segments);
		for (int i = 0; i < Num_segments; ++i) {
			auto &segment = receipt.source.segments[i];
			for (unsigned j = 0; j < 8; ++j) segment.vertices[j] = Segments[i].verts[j];
			for (unsigned j = 0; j < 6; ++j) {
				segment.children[j] = Segments[i].children[j];
				segment.sides[j].corners = xl_compat::classic_side_corners[j];
			}
		}
	}
	receipt.world = xl_compat::compile_particle_world(receipt.source);
	{
		auto stale = receipt.source;
		stale.vertices[0][0] ^= 1;
		bool rejected = false;
		try {
			(void) xl_compat::compile_particle_world(stale);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Particle world accepted stale geometry with matching counts");
	}
	for (std::size_t i = 0; i < receipt.world.segments.size(); ++i) {
		const auto &segment = receipt.world.segments[i];
		const auto query = [&](const xl_compat::ParticlePoint &position) {
			xl_particle_world_receipt::Query result;
			result.position = position;
			result.start = int(i);
			result.skybox = segment.skybox;
			result.located = receipt.world.locate(position, segment.skybox, int(i));
			xl_compat::FireParticle particle;
			particle.position = position;
			particle.segment = int(i);
			const auto normal = receipt.world.collide(particle);
			result.hit = bool(normal);
			result.after = particle.segment;
			if (normal) result.normal = *normal;
			receipt.queries.push_back(result);
		};
		query(segment.center);
		for (const auto &side : segment.sides) {
			query(side.anchor);
			for (int offset : { -66, 66 }) query(xl_compat::particle_detail::add(side.anchor, xl_compat::particle_detail::multiply(side.normals[0], offset)));
		}
	}
	require(native_state() == before, "Particle world compilation changed live engine state");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == simulation_calls && d_rand_get_stream_call_count(D_RNG_FX) == effects_calls, "Particle world compilation consumed simulation or effects RNG");
	out << "case " << ordinal << '\n';
	xl_particle_world_receipt::write(out, receipt);
	require(bool(out), "Particle world receipt write failed");
	std::cout << "Particle world native case " << ordinal << ": " << receipt.world.segments.size() << " segments, " << receipt.queries.size() << " queries, live state unchanged\n";
}
