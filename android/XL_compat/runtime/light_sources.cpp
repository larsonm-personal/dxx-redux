#include "light_sources.h"
#include <stdexcept>
extern "C" {
#include "gameseg.h"
#include "mission.h"
#include "wall.h"
#include "switch.h"
}

namespace xl_compat
{
namespace
{
bool trigger_target(int segment, int side)
{
	for (int i = 0; i < Num_triggers; ++i)
		for (int link = 0; link < Triggers[i].num_links; ++link)
			if (Triggers[i].seg[link] == segment && Triggers[i].side[link] == side) return true;
	return false;
}

bool textured_face(int segment, int side)
{
	const auto &seg = Segments[segment];
	const auto wall_index = seg.sides[side].wall_num;
	if (wall_index < 0 || wall_index >= Num_walls) return seg.children[side] == -1;
	const auto &wall = Walls[wall_index];
	// Pinned XL FADE_LEVELS is 31, independent of the engine's palette fade table
	if (wall.type != WALL_OPEN && (wall.type != WALL_CLOAKED || wall.cloak_value < 31)) return true;
	if (trigger_target(segment, side)) return true;
	const auto child = seg.children[side];
	if (child < 0 || child > Highest_segment_index) return true;
	const auto opposite = find_connect_side(&Segments[segment], &Segments[child]);
	if (opposite < 0) return true;
	const auto opposite_wall = Segments[child].sides[opposite].wall_num;
	return opposite_wall < 0 || opposite_wall >= Num_walls || trigger_target(child, opposite);
}
} // namespace

std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureColorTable &colors)
{
	if (!Current_mission || Current_mission->descent_version != 1)
		throw std::runtime_error("Classic D1 geometry light materials require a D1 mission");
	ClassicFaceLightMaterials materials(brightness, colors);
	std::vector<ClassicGeometryLight> result;
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		for (int side = 0; side < 6; ++side) {
			if (!textured_face(segment, side)) continue;
			const auto &surface = Segments[segment].sides[side];
			const auto material = materials.face(surface.tmap_num, surface.tmap_num2 & 0x3fff, false);
			if (material) {
				auto geometry = compile_classic_lighting_side(segment, side);
				const auto direction = geometry_emission_direction(geometry.fixed_normals);
				result.push_back({ segment, side, geometry, *material, direction });
			}
		}
	return result;
}
} // namespace xl_compat
