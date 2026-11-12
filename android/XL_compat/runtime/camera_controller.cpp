#include "camera_save.h"
#include "camera_controller.h"
#include "camera_renderer.h"
#include "../camera_bindings.h"
#include "preview.h"
#include "resource_validation.h"
#include "slow_motion_runtime.h"
#include <SDL.h>
#include <vector>
#include <limits>
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
	if (state.owner < 0 || state.owner > Highest_object_index || state.signature < 0) return false;
	const auto &actor = Objects[state.owner];
	return actor.signature == state.signature && actor.type == state.type && actor.id == state.id &&
	       (state.type != 16 || (actor.control_type == 18 && actor.movement_type == MT_NONE));
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
			if (actor.type != 16 && actor.type != OBJ_ROBOT && actor.type != 20)
				throw std::runtime_error("Camera attachment owner type is not qualified");
			state.signature = actor.signature;
			state.type = actor.type;
			state.id = actor.id;
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
	xl_camera_renderer_reset();
	// Source restores into uncleared authored slots; CT_CAMERA and MT_NONE have
	// no serialized control/physics payload. Redux clears those slots instead
	for (const auto &source : authored) {
		auto &actor = Objects[source.owner];
		actor.ctype.ai_info.behavior = source.actor.ctype.ai_info.behavior;
		actor.mtype = source.actor.mtype;
	}
	for (auto &entry : cameras)
		if (entry.owner >= 0 && entry.type == 16) {
			entry.signature = Objects[entry.owner].signature;
			entry.rotation = {};
		}
	return 1;
}
bool xl_camera_controller_get(int owner, XlCameraControllerState &state)
{
	if (!xl_level_extensions_active() || owner < 0 || std::size_t(owner) >= owners.size() || owners[owner] < 0) return false;
	auto &entry = cameras[owners[owner]];
	if (!live(entry)) {
		entry.signature = -1;
		return false;
	}
	state = entry;
	return true;
}
nlohmann::json xl_camera_attachments_snapshot()
{
	auto saved = nlohmann::json::array();
	for (std::size_t owner = 0; owner < owners.size(); ++owner)
		if (owners[owner] >= 0) {
			XlCameraControllerState state;
			saved.push_back({ int(owner), xl_camera_controller_get(int(owner), state) ? state.signature : -1 });
		}
	return saved;
}
nlohmann::json xl_camera_trace_snapshot()
{
	// The save/get helpers may retire stale signatures. Diagnostics never do
	auto rows = nlohmann::json::array();
	for (const auto &entry : cameras) {
		const bool valid = live(entry);
		const int brakes = valid && entry.type == 16 ? Objects[entry.owner].mtype.phys_info.brakes : 0;
		rows.push_back({ { "monitor", entry.monitor }, { "owner", entry.owner }, { "signature", entry.signature }, { "type", entry.type }, { "id", entry.id }, { "live", valid }, { "original", entry.original }, { "angle", entry.rotation.angle }, { "delta", entry.rotation.delta }, { "t0", entry.rotation.t0 }, { "brakes", brakes } });
	}
	return { { "version", 1 }, { "ticks", frame_ticks }, { "states", rows } };
}
bool xl_camera_attachments_restore(const nlohmann::json &saved, bool apply)
{
	try {
		if (!saved.is_array()) return false;
		std::size_t row_index = 0;
		for (std::size_t owner = 0; owner < owners.size(); ++owner) {
			if (owners[owner] < 0) continue;
			if (row_index >= saved.size()) return false;
			const auto &row = saved[row_index++];
			if (!row.is_array() || row.size() != 2 || !row[0].is_number_integer() || row[0] != int(owner) || !row[1].is_number_integer() ||
			    (row[1].is_number_unsigned() && row[1].get<std::uint64_t>() > (std::numeric_limits<int>::max)())) return false;
			const auto signature = row[1].get<std::int64_t>();
			if (signature < -1 || signature > (std::numeric_limits<int>::max)()) return false;
			auto state = cameras[owners[owner]];
			state.signature = int(signature);
			if (signature >= 0 && !live(state)) return false;
		}
		if (row_index != saved.size()) return false;
		if (apply) {
			for (const auto &row : saved)
				for (auto &entry : cameras)
					if (entry.owner == row[0].get<int>()) {
						entry.signature = row[1].get<int>();
						entry.rotation = {};
					}
			xl_camera_renderer_reset();
		}
		return true;
	} catch (const nlohmann::json::exception &) {
		return false;
	}
}
extern "C" void xl_camera_controller_begin_frame(void)
{
	xl_hitbox_begin_frame();
	if (xl_level_extensions_active()) {
		input_demo_environment_clock clock{};
		xl_input_demo_observe("camera_frame", -1, &clock);
		frame_ticks = clock.pickup_ms;
	} else frame_ticks = SDL_GetTicks();
}
extern "C" unsigned xl_camera_controller_ticks(void)
{
	return frame_ticks;
}
extern "C" void xl_camera_controller_frame(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || std::size_t(owner) >= owners.size() || owners[owner] < 0) return;
	auto &entry = cameras[owners[owner]];
	if (entry.type != 16) return;
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
