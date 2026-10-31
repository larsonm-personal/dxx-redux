#ifndef DXX_XL_SKYBOX_RENDER_H
#define DXX_XL_SKYBOX_RENDER_H
#include "fixed_angle.h"
#include "fog.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace xl_compat
{
struct SkyboxRenderSide {
	std::array<int, 3> anchor{};
	std::array<std::array<int, 3>, 2> normals{};
	bool quad = false;
};

// Quads use SideCenter; split sides use the source expanded triangle anchor
inline bool skybox_face_visible(const SkyboxRenderSide &side, const std::array<int, 3> &eye)
{
	std::array<int, 3> direction;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto value = std::int64_t(eye[axis]) - side.anchor[axis];
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL skybox eye distance exceeds fixed range");
		direction[axis] = int(value);
	}
	return fixed_vector_dot(side.normals[0], direction) >= 0 ||
	       (!side.quad && fixed_vector_dot(side.normals[1], direction) >= 0);
}

inline bool skybox_render_enabled(bool have_skybox, bool automap, bool automap_skybox)
{
	return have_skybox && (!automap || automap_skybox);
}

// SetupCoronaFaces excludes skybox and outdoors segments before face selection
inline bool skybox_corona_segment_enabled(int function, int properties)
{
	return function != 10 && !(properties & 16);
}

// Source WALL_OPEN=4, WALL_CLOAKED=7 and FADE_LEVELS=31 (not Redux's 34)
inline bool skybox_wall_invisible(int type, int cloak, bool target, bool opposite, bool opposite_target)
{
	return (type == 4 || (type == 7 && cloak >= 31)) && !target && opposite && !opposite_target;
}

inline bool skybox_side_textured(const Inventory &level, int segment, int side)
{
	const auto &owner = level.segments.at(segment);
	const int index = owner.sides.at(side).wall;
	if (index < 0) return owner.children.at(side) == -1;
	const auto &wall = level.walls.at(index);
	const int child = owner.children.at(side);
	if (child < 0 || std::size_t(child) >= level.segments.size()) return true;
	const auto target = [&](int s, int f) {
		for (const auto &trigger : level.triggers)
			if (std::find(trigger.targets.begin(), trigger.targets.end(), std::array<int, 2>{ s, f }) != trigger.targets.end()) return true;
		return false;
	};
	const auto &other = level.segments[child];
	for (int opposite = 0; opposite < 6; ++opposite)
		if (other.children[opposite] == segment)
			return !skybox_wall_invisible(wall.type, wall.cloak, target(segment, side), other.sides[opposite].wall >= 0, target(child, opposite));
	return true;
}

inline bool skybox_colored_only_boundary(const Inventory &level, int segment, int side)
{
	const auto &owner = level.segments.at(segment);
	return owner.function == 10 && fog_type(level, segment) != fog_type(level, owner.children.at(side)) && !skybox_side_textured(level, segment, side);
}

// Source traverses every skybox segment, not only the ordinary visibility list
template <class First, class Next, class Draw>
void visit_skybox_objects(const std::vector<int> &segments, int object_count, First first, Next next, Draw draw)
{
	int visited = 0;
	for (const int segment : segments)
		for (int object = first(segment); object != -1; object = next(object)) {
			if (object < 0 || object >= object_count || ++visited > object_count) throw std::runtime_error("XL skybox object list is invalid or cyclic");
			draw(object);
		}
}
} // namespace xl_compat
#endif
