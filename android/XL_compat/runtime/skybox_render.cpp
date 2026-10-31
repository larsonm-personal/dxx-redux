#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "skybox_render.h"
#include "../skybox_render.h"
#include "geometry.h"
#include "preview.h"
#include <algorithm>
#include <memory>
#include <vector>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "segpoint.h"
#include "../../../d2/3d/globvars.h"
#include "game.h"
#include "render.h"
#include "texmap.h"
#include "wall.h"
#include "automap.h"
#include "ogl_init.h"
#include "dxxerror.h"
#include "console.h"
void render_face(int, int, int, int *, int, int, uvl *, int);
void do_render_object(int, int);
}
#undef min
#undef max

namespace
{
using namespace xl_compat;
struct Face {
	int segment, side;
	SkyboxRenderSide visibility;
	std::array<std::array<unsigned, 3>, 2> triangles;
};
struct World {
	std::vector<bool> segments;
	std::vector<int> order;
	std::vector<Face> faces;
};
std::unique_ptr<World> world;
bool drawing = false;
bool drawing_objects = false;

} // namespace

void xl_compat::bind_skybox_render(const Inventory &level)
{
	world.reset();
	if (std::none_of(level.segments.begin(), level.segments.end(), [](const Segment &s) { return s.function == 10; })) return;
	if (level.segments.size() != std::size_t(Highest_segment_index + 1)) throw std::runtime_error("XL skybox source does not match loaded mine");
	auto next = std::make_unique<World>();
	next->segments.resize(level.segments.size());
	for (std::size_t i = 0; i < level.segments.size(); ++i) {
		if (!(next->segments[i] = level.segments[i].function == 10)) continue;
		next->order.push_back(int(i));
		for (unsigned side = 0; side < 6; ++side) {
			if (!skybox_side_textured(level, int(i), int(side))) continue;
			const auto geometry = compile_classic_lighting_side(int(i), int(side));
			const int corner = geometry.split_13 ? 1 : 0;
			const auto &anchor = Vertices[Segments[i].verts[classic_side_corners[side][corner]]];
			// Supported four-corner sides use XL SetupAsTriangles, including planar sides
			next->faces.push_back({ int(i), int(side), { { anchor.x, anchor.y, anchor.z }, geometry.fixed_normals, false }, geometry.face.triangles });
		}
	}
	world = std::move(next);
}

extern "C" void xl_skybox_render_clear(void)
{
	world.reset();
	drawing = false;
	drawing_objects = false;
}
extern "C" int xl_skybox_renderer_ready(void)
{
	return gl_initialized != 0;
}
extern "C" int xl_skybox_segment(int segment)
{
	return world && segment >= 0 && std::size_t(segment) < world->segments.size() && world->segments[segment];
}
extern "C" int xl_skybox_rendering(void)
{
	return drawing;
}

extern "C" int xl_skybox_object_pass(void)
{
	return drawing_objects;
}

extern "C" int xl_skybox_collect_view_objects(short *objects, int capacity)
{
	if (!world || !xl_level_extensions_active()) return 0;
	if (!objects || capacity <= 0) return -1;
	try {
		int count = 0;
		visit_skybox_objects(world->order, Highest_object_index + 1, [](int segment) { return Segments[segment].objects; }, [](int object) { return Objects[object].next; }, [&](int object) {
			                    if (Objects[object].type != OBJ_ROBOT && Objects[object].type != OBJ_PLAYER) return;
			                    // Match native renderer's historical upper-half overwrite
			                    if (count >= capacity) count /= 2;
			                    objects[count++] = short(object); });
		return count;
	} catch (const std::exception &) {
		return -1;
	}
}

extern "C" void xl_skybox_render_objects(int window)
{
	if (!world || !xl_level_extensions_active()) return;
	try {
		struct Restore {
			bool objects = drawing_objects;
			int left = Window_clip_left, right = Window_clip_right, top = Window_clip_top, bottom = Window_clip_bot;
			int depth = Current_seg_depth, linear = Max_linear_depth;
			Restore()
			{
				drawing_objects = true;
				Window_clip_left = Window_clip_top = 0;
				Window_clip_right = Canvas_width - 1;
				Window_clip_bot = Canvas_height - 1;
				Current_seg_depth = 0;
				Max_linear_depth = Max_linear_depth_objects;
			}
			~Restore()
			{
				drawing_objects = objects;
				Window_clip_left = left;
				Window_clip_right = right;
				Window_clip_top = top;
				Window_clip_bot = bottom;
				Current_seg_depth = depth;
				Max_linear_depth = linear;
			}
		} restore;
		visit_skybox_objects(world->order, Highest_object_index + 1, [](int segment) { return Segments[segment].objects; }, [](int object) { return Objects[object].next; }, [window](int object) { do_render_object(object, window); });
	} catch (const std::exception &error) {
		Error("XL skybox objects: %s", error.what());
	}
}

void xl_compat::skybox_render_native_controls(const Inventory &original)
{
	struct Restore {
		std::unique_ptr<World> saved = std::move(world);
		~Restore()
		{
			world = std::move(saved);
		}
	} restore;
	Inventory derived = original;
	unsigned faces = 0;
	for (unsigned mask = 0; mask < 3; ++mask) {
		for (std::size_t i = 0; i < derived.segments.size(); ++i) derived.segments[i].function = mask == 2 || (mask == 1 && i % 2) ? 10 : 0;
		bind_skybox_render(derived);
		if (drawing || (mask == 0 && world)) throw std::runtime_error("Skybox render binding retained an empty owner or entered a draw");
		for (std::size_t i = 0; i < derived.segments.size(); ++i)
			if (bool(xl_skybox_segment(int(i))) != (derived.segments[i].function == 10)) throw std::runtime_error("Skybox render membership differs from loaded source");
		if (xl_skybox_segment(-1) || xl_skybox_segment(int(derived.segments.size()))) throw std::runtime_error("Skybox render accepted an out-of-range segment");
		if (!world) continue;
		int previous = -1;
		for (const auto &face : world->faces) {
			if (!xl_skybox_segment(face.segment) || face.segment * 6 + face.side <= previous) throw std::runtime_error("Skybox face list lost source segment/side order");
			previous = face.segment * 6 + face.side;
			for (const auto &triangle : face.triangles)
				for (const auto corner : triangle)
					if (corner >= 4) throw std::runtime_error("Skybox face references an invalid source corner");
			++faces;
		}
	}
	if (!faces) throw std::runtime_error("Skybox render binding did not exercise loaded faces");
	xl_skybox_render_clear();
	if (world || drawing) throw std::runtime_error("Skybox render owner did not clear");
	con_printf(CON_NORMAL, "XL skybox render binding: %u loaded faces across mixed/all-skybox controls; order, membership and clear passed", faces);
}

extern "C" void xl_skybox_render_scene(void)
{
	// Redux's line automap has no skybox option
	if (!world || !xl_level_extensions_active() || !skybox_render_enabled(true, Automap_active != 0, false)) return;
	try {
		struct Restore {
			GLboolean depth_write, depth_test;
			GLint depth_function;
			int left = Window_clip_left, right = Window_clip_right, top = Window_clip_top, bottom = Window_clip_bot;
			int depth = Current_seg_depth, linear = Max_linear_depth;
			Restore()
			{
				glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
				glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
				depth_test = glIsEnabled(GL_DEPTH_TEST);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				glDepthMask(GL_TRUE);
				drawing = true;
				Window_clip_left = Window_clip_top = 0;
				Window_clip_right = grd_curcanv->cv_bitmap.bm_w - 1;
				Window_clip_bot = grd_curcanv->cv_bitmap.bm_h - 1;
				Current_seg_depth = 0;
			}
			~Restore()
			{
				drawing = false;
				glDepthMask(depth_write);
				glDepthFunc(depth_function);
				if (!depth_test) glDisable(GL_DEPTH_TEST);
				Window_clip_left = left;
				Window_clip_right = right;
				Window_clip_top = top;
				Window_clip_bot = bottom;
				Current_seg_depth = depth;
				Max_linear_depth = linear;
			}
		} restore;
		const std::array<int, 3> eye{ View_position.x, View_position.y, View_position.z };
		for (const auto &face : world->faces) {
			if (!skybox_face_visible(face.visibility, eye)) continue;
			auto &segment = Segments[face.segment];
			auto &side = segment.sides[face.side];
			int ids[4];
			for (unsigned i = 0; i < 4; ++i) {
				ids[i] = segment.verts[classic_side_corners[face.side][i]];
				g3_rotate_point(&Segment_points[ids[i]], &Vertices[ids[i]]);
			}
			const int doorway = WALL_IS_DOORWAY(&segment, face.side);
			if (!(doorway & WID_RENDER_FLAG)) continue;
			for (const auto &triangle : face.triangles) {
				int vertices[3];
				uvl uvls[3];
				for (unsigned i = 0; i < 3; ++i) {
					vertices[i] = ids[triangle[i]];
					uvls[i] = side.uvls[triangle[i]];
				}
				render_face(face.segment, face.side, 3, vertices, side.tmap_num, side.tmap_num2, uvls, doorway);
			}
		}
	} catch (const std::exception &error) {
		Error("XL skybox rendering: %s", error.what());
	}
}
