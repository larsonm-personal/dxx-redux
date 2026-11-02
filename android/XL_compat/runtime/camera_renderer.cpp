#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "camera_renderer.h"
#include "camera_scene.h"
#include "camera_controller.h"
#include "preview.h"
#include "../camera_bindings.h"
#include "../camera_schedule.h"
#include "../camera_alignment.h"
#include "../camera_monitor.h"
#include <memory>
#include <algorithm>
#include <SDL.h>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "gameseg.h"
#include "game.h"
#include "render.h"
#include "multi.h"
#include "endlevel.h"
#include "newdemo.h"
#include "ogl_init.h"
#include "dxxerror.h"
}
#undef min
#undef max
#if !defined(OGLES) || defined(ANDROID)
#include "camera_target_gpu.h"
namespace
{
using namespace xl_compat;
struct Monitor {
	CameraBinding binding;
	int signature = -1;
	CameraTargetGpu target;
	CameraAlignment alignment;
	ogl_texture texture{};
	grs_bitmap bitmap{};
};
std::vector<std::unique_ptr<Monitor>> monitors;
std::vector<ScheduledCamera> schedules;
std::vector<int> faces;
Monitor *active = nullptr;
CameraMonitorSelection selection;
std::array<CameraUV, 8> active_uv{};
int active_count = 0, active_unit = 0;
int screen_width = 0, screen_height = 0;
// Pinned ordinary-camera defaults: fit to wall, unrestricted FPS, half resolution
constexpr bool high_resolution = false;
int face_index(int segment, int side)
{
	if (!xl_level_extensions_active() || xl_camera_scene_active() || segment < 0 || side < 0 || side >= 6) return -1;
	const auto index = std::size_t(segment) * 6 + side;
	return index < faces.size() ? faces[index] : -1;
}
void fail(const char *operation, const std::exception &error)
{
	Error("XL camera %s failed: %s", operation, error.what());
}
} // namespace
void xl_camera_renderer_load(const xl_compat::Inventory &level)
{
	const auto bindings = xl_compat::camera_bindings(level);
	std::vector<std::unique_ptr<Monitor>> prepared;
	for (const auto &binding : bindings.cameras) {
		if (binding.object < 0) throw std::runtime_error("Wall camera viewer placement remains unqualified");
		if (binding.object > Highest_object_index || Objects[binding.object].type != 16)
			throw std::runtime_error("Camera monitor owner is not an installed camera");
		auto monitor = std::make_unique<Monitor>();
		monitor->binding = binding;
		monitor->signature = Objects[binding.object].signature;
		prepared.push_back(std::move(monitor));
	}
	xl_camera_renderer_clear();
	monitors.swap(prepared);
	faces = bindings.faces;
	schedules.resize(monitors.size());
}
extern "C" void xl_camera_renderer_clear(void)
{
	xl_camera_monitor_end();
	monitors.clear();
	schedules.clear();
	faces.clear();
	screen_width = screen_height = 0;
}
extern "C" void xl_camera_renderer_reset(void)
{
	xl_camera_monitor_end();
	for (auto &monitor : monitors) {
		monitor->target.discard(false);
		monitor->alignment.invalidate();
		monitor->signature = Objects[monitor->binding.object].signature;
	}
	schedules.assign(monitors.size(), {});
}
extern "C" void xl_camera_renderer_frame(void)
{
	if (monitors.empty() || !xl_level_extensions_active() || xl_camera_scene_active() || !gl_initialized ||
	    !grd_curscreen || (Game_mode & GM_MULTI) || Endlevel_sequence || Newdemo_state != ND_STATE_NORMAL) return;
	try {
		const int width = grd_curscreen->sc_w, height = grd_curscreen->sc_h;
		if (width != screen_width || height != screen_height) {
			xl_camera_renderer_reset();
			screen_width = width;
			screen_height = height;
		}
		for (std::size_t i = 0; i < monitors.size(); ++i) {
			const auto &monitor = *monitors[i];
			const auto &actor = Objects[monitor.binding.object];
			if (actor.type != 16 || actor.signature != monitor.signature)
				throw std::runtime_error("Camera monitor owner identity changed");
			schedules[i].owner_flags = actor.flags;
		}
		const auto ticks = SDL_GetTicks();
		const int chosen = select_camera(schedules, ticks, ticks, 0, true, false);
		if (chosen < 0) return;
		auto &monitor = *monitors[chosen];
		const auto spec = camera_target_spec(width, height, high_resolution);
		capture_camera_scene(monitor.target, spec, Objects[monitor.binding.object], Render_zoom, true, true);
		monitor.texture = {};
		monitor.texture.handle = monitor.target.texture();
		monitor.texture.w = monitor.texture.tw = spec.width;
		monitor.texture.h = monitor.texture.th = spec.height;
		monitor.texture.u = monitor.texture.v = 1;
		monitor.texture.internalformat = GL_RGB8;
		monitor.texture.format = GL_RGB;
		monitor.texture.xl_rgba = 1;
		monitor.bitmap = {};
		monitor.bitmap.bm_w = short(spec.width);
		monitor.bitmap.bm_h = short(spec.height);
		monitor.bitmap.bm_type = BM_RGBA;
		monitor.bitmap.gltexture = &monitor.texture;
	} catch (const std::exception &error) {
		fail("capture", error);
	}
}
extern "C" int xl_camera_monitor_face(int segment, int side)
{
	return face_index(segment, side) >= 0;
}
extern "C" void xl_camera_monitor_begin(int segment, int side, int count, const int *vertices, int overlay)
{
	xl_camera_monitor_end();
	const int index = face_index(segment, side);
	if (index < 0) return;
	try {
		if (!vertices || count < 3 || count > int(active_uv.size())) throw std::runtime_error("Invalid camera monitor polygon");
		auto &monitor = *monitors[index];
		CameraMonitorInput input;
		input.exists = input.enabled = input.base = true;
		input.valid = input.texture = monitor.target.valid();
		input.overlay_texture = overlay & 0x3fff;
		input.fit_to_wall = true;
		selection = camera_monitor_selection(input);
		if (selection.visible) schedules[index].schedule.visible = true;
		if (!selection.align) return;
		std::array<CameraUV, 6> authored{};
		const auto &face = Segments[segment].sides[side];
		for (unsigned i = 0; i < 4; ++i) authored[i] = { float(face.uvls[i].u) / F1_0, float(face.uvls[i].v) / F1_0 };
		monitor.alignment.align(authored, false, false, monitor.binding.mirror, true, high_resolution,
		                        screen_width, screen_height, monitor.texture.w, monitor.texture.h);
		int corners[4];
		get_side_verts(corners, segment, side);
		for (int i = 0; i < count; ++i) {
			const auto corner = std::find(corners, corners + 4, vertices[i]);
			if (corner == corners + 4) throw std::runtime_error("Camera monitor vertex is outside its face");
			active_uv[i] = monitor.alignment.coordinates[corner - corners];
		}
		active = &monitor;
		active_count = count;
		active_unit = input.overlay_texture ? 1 : 0;
	} catch (const std::exception &error) {
		fail("monitor setup", error);
	}
}
extern "C" grs_bitmap *xl_camera_monitor_bitmap(int overlay, grs_bitmap *original)
{
	if (!active) return original;
	const bool replace = overlay ? selection.image == CameraMonitorImage::overlay : selection.image == CameraMonitorImage::base;
	return replace ? &active->bitmap : original;
}
extern "C" int xl_camera_monitor_is_bitmap(const grs_bitmap *bitmap)
{
	return active && bitmap == &active->bitmap;
}
extern "C" void xl_camera_monitor_uv(int overlay, int count, float *coordinates)
{
	if (!active || overlay != active_unit) return;
	if (!coordinates || count != active_count) Error("Camera monitor texture coordinates lost polygon identity");
	for (int i = 0; i < count; ++i) {
		coordinates[2 * i] = active_uv[i].u;
		coordinates[2 * i + 1] = active_uv[i].v;
	}
}
extern "C" void xl_camera_monitor_end(void)
{
	active = nullptr;
	active_count = 0;
	selection = {};
}
#else
void xl_camera_renderer_load(const xl_compat::Inventory &level)
{
	if (!xl_compat::camera_bindings(level).cameras.empty()) throw std::runtime_error("Camera monitors require a capable GL renderer");
}
extern "C" void xl_camera_renderer_clear(void) {}
extern "C" void xl_camera_renderer_reset(void) {}
extern "C" void xl_camera_renderer_frame(void) {}
extern "C" int xl_camera_monitor_face(int, int)
{
	return 0;
}
extern "C" void xl_camera_monitor_begin(int, int, int, const int *, int) {}
extern "C" grs_bitmap *xl_camera_monitor_bitmap(int, grs_bitmap *original)
{
	return original;
}
extern "C" int xl_camera_monitor_is_bitmap(const grs_bitmap *)
{
	return 0;
}
extern "C" void xl_camera_monitor_uv(int, int, float *) {}
extern "C" void xl_camera_monitor_end(void) {}
#endif
