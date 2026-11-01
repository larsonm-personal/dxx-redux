#include "camera_controller.h"
#include "../camera_bindings.h"
#include "preview.h"
#include "resource_validation.h"
#include <SDL.h>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "dxxerror.h"
}
namespace
{
std::vector<XlCameraControllerState> cameras;
std::vector<int> owners;
struct AuthoredCamera {
	int owner;
	object actor;
};
std::vector<AuthoredCamera> authored;
// Source nSDLTicks is a cached game clock and survives camera/level resets
unsigned frame_ticks = 0;
// Pinned gameoptions.cpp default; no camera speed setting has been exposed yet
constexpr int camera_speed = 5000;
xl_compat::CameraMatrix matrix(const vms_matrix &m)
{
	return { { { m.rvec.x, m.rvec.y, m.rvec.z }, { m.uvec.x, m.uvec.y, m.uvec.z }, { m.fvec.x, m.fvec.y, m.fvec.z } } };
}
bool live(const XlCameraControllerState &state)
{
	return state.owner >= 0 && state.owner <= Highest_object_index && Objects[state.owner].signature == state.signature &&
	       Objects[state.owner].type == 16 && Objects[state.owner].control_type == 18 && Objects[state.owner].movement_type == MT_NONE;
}
} // namespace
extern "C" void xl_camera_controller_clear(void)
{
	cameras.clear();
	owners.clear();
	authored.clear();
}
void xl_camera_controller_load(const xl_compat::Inventory &level)
{
	if (level.segments.size() != std::size_t(Num_segments)) throw std::runtime_error("Camera controller geometry does not match loaded level");
	const auto bindings = xl_compat::camera_bindings(level);
	std::vector<AuthoredCamera> prepared_authored;
	for (std::size_t i = 0; i < level.objects.size(); ++i)
		if (level.objects[i].type == 16) {
			if (i > std::size_t(Highest_object_index) || Objects[i].type != 16 || Objects[i].control_type != 18 || Objects[i].movement_type != MT_NONE)
				throw std::runtime_error("Authored camera does not match loaded object");
			prepared_authored.push_back({ int(i), Objects[i] });
		}
	std::vector<XlCameraControllerState> prepared;
	for (const auto &binding : bindings.cameras) {
		XlCameraControllerState state;
		state.monitor = int(prepared.size());
		state.owner = binding.object;
		if (state.owner >= 0) {
			if (state.owner > Highest_object_index) throw std::runtime_error("Camera controller owner is missing");
			const auto &actor = Objects[state.owner];
			state.signature = actor.signature;
			if (!live(state)) throw std::runtime_error("Camera controller owner exceeds qualified object profile");
			state.original = matrix(actor.orient);
			xl_compat::camera_orientation(state.original, 0);
		}
		prepared.push_back(state);
	}
	auto prepared_owners = bindings.objects;
	cameras.swap(prepared);
	owners.swap(prepared_owners);
	authored.swap(prepared_authored);
}
extern "C" int xl_camera_controller_restore(int apply)
{
	if (!xl_level_extensions_active()) return authored.empty();
	std::size_t count = 0;
	for (int i = 0; i <= Highest_object_index; ++i) count += Objects[i].type == 16;
	if (count != authored.size()) return 0;
	for (const auto &source : authored) {
		if (source.owner > Highest_object_index) return 0;
		const auto &actor = Objects[source.owner];
		if (actor.type != 16 || actor.control_type != 18 || actor.movement_type != MT_NONE || actor.signature < 0 ||
		    actor.id != source.actor.id || actor.render_type != source.actor.render_type || actor.rtype.pobj_info.model_num != source.actor.rtype.pobj_info.model_num ||
		    actor.lifeleft != IMMORTAL_TIME || actor.segnum < 0 || actor.segnum >= Num_segments) return 0;
	}
	if (!apply) return 1;
	// Source restores into uncleared authored slots; CT_CAMERA and MT_NONE have
	// no serialized control/physics payload. Redux clears those slots instead
	for (const auto &source : authored) {
		auto &actor = Objects[source.owner];
		actor.ctype.ai_info.behavior = source.actor.ctype.ai_info.behavior;
		actor.mtype = source.actor.mtype;
	}
	for (auto &entry : cameras)
		if (entry.owner >= 0) {
			entry.signature = Objects[entry.owner].signature;
			entry.rotation = {};
		}
	return 1;
}
bool xl_camera_controller_get(int owner, XlCameraControllerState &state)
{
	if (!xl_level_extensions_active() || owner < 0 || std::size_t(owner) >= owners.size() || owners[owner] < 0) return false;
	const auto &entry = cameras[owners[owner]];
	if (!live(entry)) return false;
	state = entry;
	return true;
}
extern "C" void xl_camera_controller_begin_frame(void)
{
	xl_hitbox_begin_frame();
	frame_ticks = SDL_GetTicks();
}
extern "C" unsigned xl_camera_controller_ticks(void)
{
	return frame_ticks;
}
extern "C" void xl_camera_controller_frame(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || std::size_t(owner) >= owners.size() || owners[owner] < 0) return;
	auto &entry = cameras[owners[owner]];
	if (!live(entry)) Error("Camera controller identity changed outside qualified lifetime");
	auto &actor = Objects[owner];
	try {
		auto next = entry.rotation;
		auto brakes = actor.mtype.phys_info.brakes;
		if (!next.advance(true, actor.ctype.ai_info.behavior, frame_ticks, camera_speed, brakes)) return;
		const auto oriented = xl_compat::camera_orientation(entry.original, next.angle);
		actor.orient = { { oriented[0][0], oriented[0][1], oriented[0][2] }, { oriented[1][0], oriented[1][1], oriented[1][2] }, { oriented[2][0], oriented[2][1], oriented[2][2] } };
		actor.mtype.phys_info.brakes = brakes;
		entry.rotation = next;
	} catch (const std::exception &error) {
		Error("Cannot rotate XL camera: %s", error.what());
	}
}
