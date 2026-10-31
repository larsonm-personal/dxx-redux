#ifndef DXX_XL_SKYBOX_RENDER_H
#define DXX_XL_SKYBOX_RENDER_H
#include "fixed_angle.h"
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

// Source WALL_OPEN=4, WALL_CLOAKED=7 and FADE_LEVELS=31 (not Redux's 34)
inline bool skybox_wall_invisible(int type, int cloak, bool target, bool opposite, bool opposite_target)
{
	return (type == 4 || (type == 7 && cloak >= 31)) && !target && opposite && !opposite_target;
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
