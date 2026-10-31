#ifndef DXX_XL_PARTICLE_WORLD_H
#define DXX_XL_PARTICLE_WORLD_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "particle_fire.h"
#include <optional>
#include <map>

namespace xl_compat
{
struct ParticleSide {
	ParticlePoint anchor{};
	std::array<ParticlePoint, 2> normals{};
	unsigned faces = 0;
	int child = -1;
	bool pokes_out = false;
};
struct ParticleSegment {
	std::array<ParticleSide, 6> sides{};
	ParticlePoint center{};
	std::array<ParticlePoint, 2> bounds{};
	int inner_radius = 0, outer_radius = 0;
	bool skybox = false;
};

inline ParticlePoint particle_difference(const ParticlePoint &a, const ParticlePoint &b)
{
	ParticlePoint result;
	for (unsigned i = 0; i < 3; ++i) result[i] = particle_detail::signed_checked(std::int64_t(a[i]) - b[i]);
	return result;
}
inline int particle_plane_distance(const ParticlePoint &point, const ParticleSide &side, unsigned face)
{
	return fixed_vector_dot(particle_difference(point, side.anchor), side.normals.at(face));
}

struct ParticleGrid {
	ParticlePoint minimum{}, dimensions{};
	int cell_size = 40;
	std::map<std::int64_t, std::vector<int>> cells;
	std::int64_t size() const
	{
		return std::int64_t(dimensions[0]) * dimensions[1] * dimensions[2];
	}
	std::int64_t index(const ParticlePoint &cell) const
	{
		return (std::int64_t(cell[2]) * dimensions[1] + cell[1]) * dimensions[0] + cell[0];
	}
	ParticlePoint cell(const ParticlePoint &point) const
	{
		auto result = particle_difference(point, minimum);
		for (auto &coordinate : result) {
			// XL first truncates fixed division, then floors the fixed result
			coordinate /= cell_size;
			coordinate = coordinate / 65536 - (coordinate < 0 && coordinate % 65536 ? 1 : 0);
		}
		return result;
	}
	void build(const std::vector<ParticleSegment> &segments, bool skybox)
	{
		cells.clear();
		dimensions = {};
		cell_size = skybox ? 80 : 40;
		minimum = { INT32_MAX, INT32_MAX, INT32_MAX };
		ParticlePoint maximum{ INT32_MIN, INT32_MIN, INT32_MIN };
		bool found = false;
		for (const auto &segment : segments)
			if (segment.skybox == skybox) {
				found = true;
				for (unsigned axis = 0; axis < 3; ++axis) {
					minimum[axis] = std::min(minimum[axis], segment.bounds[0][axis]);
					maximum[axis] = std::max(maximum[axis], segment.bounds[1][axis]);
				}
			}
		if (!found) return;
		const auto span = particle_difference(maximum, minimum);
		for (unsigned axis = 0; axis < 3; ++axis) {
			const int fixed = span[axis] / cell_size;
			dimensions[axis] = fixed / 65536 + (fixed % 65536 ? 1 : 0) + 1;
		}
		if (size() > INT32_MAX) throw std::runtime_error("XL particle grid exceeds source index range");
		for (std::size_t i = 0; i < segments.size(); ++i)
			if (segments[i].skybox == skybox) {
				const auto first = cell(segments[i].bounds[0]), last = cell(segments[i].bounds[1]);
				for (int z = first[2]; z <= last[2]; ++z)
					for (int y = first[1]; y <= last[1]; ++y)
						for (int x = first[0]; x <= last[0]; ++x) cells[index({ x, y, z })].push_back(int(i));
			}
	}
};

class ParticleWorld
{
  public:
	// The loaded XL geometry uses trunc(65536 * 0.001), not the startup 250
	static constexpr int plane_tolerance = 65;
	std::vector<ParticleSegment> segments;
	std::array<ParticleGrid, 2> grids;
	void build_grids()
	{
		grids[0].build(segments, false);
		grids[1].build(segments, true);
	}

	bool contains(int segment, const ParticlePoint &point) const
	{
		const auto &mine = segments.at(std::size_t(segment));
		const int distance = geometry_default_magnitude(particle_difference(point, mine.center));
		if (distance <= mine.inner_radius) return true;
		if (distance > mine.outer_radius) return false;
		for (const auto &side : mine.sides) {
			if (side.faces > 2) throw std::runtime_error("Invalid XL particle collision face count");
			unsigned outside = 0;
			for (unsigned face = 0; face < side.faces; ++face)
				if (particle_plane_distance(point, side, face) < -plane_tolerance) ++outside;
			if (outside && (side.pokes_out || outside == side.faces)) return false;
		}
		return true;
	}

	int locate(const ParticlePoint &point, bool skybox, int start) const
	{
		if (start >= 0 && contains(start, point)) return start;
		const auto &grid = grids[skybox ? 1 : 0];
		if (!grid.cells.empty()) {
			// Preserve source flattened-index bounds and ascending segment ordering
			const auto index = grid.index(grid.cell(point));
			if (index < 0 || index >= grid.size()) return -1;
			const auto found = grid.cells.find(index);
			if (found != grid.cells.end())
				for (const int candidate : found->second)
					if (contains(candidate, point)) return candidate;
			return -1;
		}
		// Source exhaustive fallback, with the supplied segment checked first
		for (std::size_t i = 0; i < segments.size(); ++i)
			if (segments[i].skybox == skybox && contains(int(i), point)) return int(i);
		return -1;
	}

	// FindSegByPos with bExhaustive=0 and zero tolerance, used by lightning lights
	int locate_connected(const ParticlePoint &point, int start) const
	{
		if (start < 0) return -1;
		if (std::size_t(start) >= segments.size()) throw std::runtime_error("Connected lookup starts outside the mine");
		if (contains(start, point)) return start;
		// Skybox tracing uses the preceding stamp before the ordinary pass advances it
		static thread_local std::vector<std::uint16_t> visited;
		static thread_local std::uint16_t stamp = 0xffff;
		if (visited.size() < segments.size()) visited.resize(segments.size());
		struct Frame {
			int segment, depth;
			std::array<int, 6> distances;
		};
		const auto trace = [&](int initial_depth) {
			std::vector<Frame> stack;
			int current = start, depth = initial_depth;
			for (;;) {
				if (current >= 0) {
					if (std::size_t(current) >= segments.size()) throw std::runtime_error("Connected lookup child is outside the mine");
					if (std::size_t(depth) < segments.size() && visited[current] != stamp) {
						visited[current] = stamp;
						Frame frame{ current, depth, {} };
						bool outside = false;
						for (unsigned side = 0; side < 6; ++side) {
							const auto &geometry = segments[current].sides[side];
							if (!geometry.faces) continue;
							if (geometry.faces > 2) throw std::runtime_error("Connected lookup has invalid side geometry");
							unsigned behind = 0;
							std::int64_t sum = 0;
							for (unsigned face = 0; face < geometry.faces; ++face) {
								const int distance = particle_plane_distance(point, geometry, face);
								if (distance < -plane_tolerance) {
									++behind;
									sum += distance;
								}
							}
							if (behind && (geometry.pokes_out || behind == geometry.faces)) {
								outside = true;
								frame.distances[side] = particle_detail::signed_checked(sum / behind);
							}
						}
						if (!outside) return current;
						stack.push_back(frame);
					}
				}
				current = -1;
				while (!stack.empty()) {
					auto &frame = stack.back();
					int chosen = -1, minimum = 0;
					for (unsigned side = 0; side < 6; ++side)
						if (segments[frame.segment].sides[side].child >= 0 && frame.distances[side] < minimum) {
							minimum = frame.distances[side];
							chosen = int(side);
						}
					if (chosen < 0) {
						stack.pop_back();
						continue;
					}
					frame.distances[chosen] = 0;
					current = segments[frame.segment].sides[chosen].child;
					depth = frame.depth + 1;
					break;
				}
				if (current < 0) return -1;
			}
		};
		if (segments[start].skybox) {
			const int found = trace(1);
			if (found >= 0) return found;
		}
		if (!++stamp) {
			std::fill(visited.begin(), visited.end(), 0);
			stamp = 1;
		}
		return trace(0);
	}

	std::optional<ParticlePoint> collide(FireParticle &particle) const
	{
		if (particle.segment < 0) return {};
		const auto &mine = segments.at(std::size_t(particle.segment));
		for (const auto &side : mine.sides) {
			if (!side.faces) continue;
			if (side.faces > 2) throw std::runtime_error("Invalid XL particle collision face count");
			unsigned inside = 0;
			std::optional<ParticlePoint> normal;
			for (unsigned face = 0; face < side.faces; ++face) {
				// Collision and point containment differ at the tolerance boundary
				if (particle_plane_distance(particle.position, side, face) > -plane_tolerance) ++inside;
				else normal = side.normals[face];
			}
			if (!inside || (inside == 1 && side.faces == 2 && side.pokes_out)) {
				if (side.child < 0) return normal;
				if (std::size_t(side.child) >= segments.size()) throw std::runtime_error("XL particle portal references an invalid segment");
				particle.segment = side.child;
				break;
			}
		}
		return {};
	}
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
