#include "input_demo_object_trace.h"
#include "input_demo_state_trace.h"
#include "input_demo_ai_trace.h"

#include <array>
#include <string>
#include <nlohmann/json.hpp>

extern "C" {
#include "ai.h"
#include "game.h"
#include "maths.h"
#include "object.h"
#include "segment.h"
}

namespace
{
using json = nlohmann::ordered_json;

json vector(const vms_vector &v)
{
	return { v.x, v.y, v.z };
}
json angles(const vms_angvec &v)
{
	return { v.p, v.b, v.h };
}

// Copy packed engine scalars into JSON values without exposing ABI padding
#define FIELD(out, value, name) (out)[#name] = static_cast<int64_t>((value).name)

json ai_state(const ai_static &a)
{
	json out = json::object();
	FIELD(out, a, behavior);
	out["flags"] = json::array();
	for (int i = 0; i < MAX_AI_FLAGS; ++i) out["flags"].push_back(static_cast<int>(a.flags[i]));
	FIELD(out, a, hide_segment);
	FIELD(out, a, hide_index);
	FIELD(out, a, path_length);
	FIELD(out, a, cur_path_index);
	FIELD(out, a, danger_laser_signature);
	FIELD(out, a, danger_laser_num);
#ifdef DXX_BUILD_DESCENT_II
	FIELD(out, a, dying_sound_playing);
	FIELD(out, a, dying_start_time);
	FIELD(out["d1_saved"], a.d1_saved, follow_path_start_seg);
	FIELD(out["d1_saved"], a.d1_saved, follow_path_end_seg);
#else
	FIELD(out, a, follow_path_start_seg);
	FIELD(out, a, follow_path_end_seg);
#endif
	return out;
}

json object_state(const object &o, int slot)
{
	json out = json::object();
	FIELD(out, o, signature);
	FIELD(out, o, type);
	FIELD(out, o, id);
	FIELD(out, o, next);
	FIELD(out, o, prev);
	FIELD(out, o, control_type);
	FIELD(out, o, movement_type);
	FIELD(out, o, render_type);
	FIELD(out, o, flags);
	FIELD(out, o, segnum);
	FIELD(out, o, attached_obj);
	FIELD(out, o, size);
	FIELD(out, o, shields);
	FIELD(out, o, contains_type);
	FIELD(out, o, contains_id);
	FIELD(out, o, contains_count);
	FIELD(out, o, matcen_creator);
	FIELD(out, o, lifeleft);
	out["pos"] = vector(o.pos);
	out["last_pos"] = vector(o.last_pos);
	out["orient"] = { vector(o.orient.rvec), vector(o.orient.uvec), vector(o.orient.fvec) };
	if (o.movement_type == MT_PHYSICS) {
		const auto &p = o.mtype.phys_info;
		auto &m = out["physics"];
		m["velocity"] = vector(p.velocity);
		m["thrust"] = vector(p.thrust);
		m["rotvel"] = vector(p.rotvel);
		m["rotthrust"] = vector(p.rotthrust);
		FIELD(m, p, mass);
		FIELD(m, p, drag);
		FIELD(m, p, brakes);
		FIELD(m, p, turnroll);
		FIELD(m, p, flags);
	} else if (o.movement_type == MT_SPINNING)
		out["spin_rate"] = vector(o.mtype.spin_rate);
	switch (o.control_type) {
		case CT_AI:
		case CT_MORPH:
			out["ai"] = ai_state(o.ctype.ai_info);
			break;
		case CT_WEAPON: {
			const auto &w = o.ctype.laser_info;
			auto &c = out["weapon"];
			FIELD(c, w, parent_type);
			FIELD(c, w, parent_num);
			FIELD(c, w, parent_signature);
			FIELD(c, w, creation_time);
			FIELD(c, w, last_hitobj);
			FIELD(c, w, track_goal);
			FIELD(c, w, multiplier);
			FIELD(c, w, creation_framecount);
			c["hitobj_list"] = json::array();
			for (int i = 0; i < MAX_OBJECTS; ++i) c["hitobj_list"].push_back(static_cast<int>(w.hitobj_list[i]));
			break;
		}
		case CT_EXPLOSION: {
			const auto &e = o.ctype.expl_info;
			auto &c = out["explosion"];
			FIELD(c, e, spawn_time);
			FIELD(c, e, delete_time);
			FIELD(c, e, delete_objnum);
			FIELD(c, e, attach_parent);
			FIELD(c, e, prev_attach);
			FIELD(c, e, next_attach);
			break;
		}
		case CT_LIGHT:
			out["light_intensity"] = static_cast<int64_t>(o.ctype.light_info.intensity);
			break;
		case CT_POWERUP: {
			const auto &p = o.ctype.powerup_info;
			auto &c = out["powerup"];
			FIELD(c, p, count);
#ifdef DXX_BUILD_DESCENT_II
			FIELD(c, p, creation_time);
			FIELD(c, p, flags);
#endif
			break;
		}
		case CT_CNTRLCEN:
			out["reactor_gun_pos"] = json::array();
			out["reactor_gun_dir"] = json::array();
			for (int i = 0; i < MAX_CONTROLCEN_GUNS; ++i) {
				out["reactor_gun_pos"].push_back(vector(o.ctype.reactor_info.gun_pos[i]));
				out["reactor_gun_dir"].push_back(vector(o.ctype.reactor_info.gun_dir[i]));
			}
			break;
	}
	if (o.type == OBJ_ROBOT) out["ai_local"] = input_demo_ai_local_trace_snapshot(slot);
	if (o.render_type == RT_POLYOBJ || o.render_type == RT_MORPH || (o.render_type == RT_NONE && o.type == OBJ_GHOST)) {
		const auto &p = o.rtype.pobj_info;
		auto &r = out["polyobj"];
		FIELD(r, p, model_num);
		FIELD(r, p, subobj_flags);
		FIELD(r, p, tmap_override);
		FIELD(r, p, alt_textures);
		r["anim_angles"] = json::array();
		for (int i = 0; i < MAX_SUBMODELS; ++i) r["anim_angles"].push_back(angles(p.anim_angles[i]));
	} else if (o.render_type == RT_FIREBALL || o.render_type == RT_HOSTAGE ||
	           o.render_type == RT_POWERUP || o.render_type == RT_WEAPON_VCLIP) {
		const auto &v = o.rtype.vclip_info;
		auto &r = out["vclip"];
		FIELD(r, v, vclip_num);
		FIELD(r, v, frametime);
		FIELD(r, v, framenum);
	}
	return out;
}
#undef FIELD
} // namespace

nlohmann::ordered_json input_demo_object_trace_snapshot(const object &value, int slot)
{
	return object_state(value, slot);
}

static int write_object_state(uint32_t frame, const char *phase, char *error, size_t error_size)
{
	static std::array<json, MAX_OBJECTS> previous;
	if (frame == 0 && !phase) previous.fill(nullptr);
	json record = { { "type", phase ? "object_boundary" : "object_state" }, { "version", 2 }, { "f", frame }, { "reset", phase || frame == 0 }, { "capacity", MAX_OBJECTS }, { "slots", json::object() } };
	// Avoid repeatedly copying earlier object snapshots as this ordered map grows
	record["slots"].get_ref<json::object_t &>().reserve(MAX_OBJECTS);
	if (phase) record["phase"] = phase;
	for (int i = 0; i < MAX_OBJECTS; ++i) {
		json current = Objects[i].type == OBJ_NONE ? json(nullptr) : object_state(Objects[i], i);
		if (phase ? !current.is_null() : current != previous[i]) record["slots"][std::to_string(i)] = current;
		if (!phase) previous[i] = std::move(current);
	}
	object_runtime_state allocator;
	object_get_runtime_state(&allocator);
	record["allocator"] = {
		{ "num_objects", allocator.num_objects }, { "highest_object_index", allocator.highest_object_index }, { "signature_seed", allocator.signature_seed }, { "homer_frame_count", allocator.homer_frame_count }, { "current_homer_frame_time", allocator.current_homer_frame_time }, { "do_homer_frame", allocator.do_homer_frame }
	};
	record["allocator"]["free_obj_list"] = json::array();
	for (int i = 0; i < MAX_OBJECTS; ++i) record["allocator"]["free_obj_list"].push_back(allocator.free_obj_list[i]);
	record["segment_heads"] = json::array();
	for (int i = 0; i <= Highest_segment_index; ++i) record["segment_heads"].push_back(static_cast<int>(Segments[i].objects));
	record["clock"] = { { "game_time", GameTime64 }, { "frame_time", FrameTime }, { "d_tick_count", d_tick_count } };
	record["rng"] = json::array();
	for (auto stream : { D_RNG_SIM, D_RNG_FX }) {
		unsigned state = 0;
		const int available = d_rand_get_stream_state(stream, &state);
		record["rng"].push_back({ { "available", available }, { "state", state }, { "calls", d_rand_get_stream_call_count(stream) } });
	}
	return input_demo_state_trace_write_json(record.dump().c_str(), error, error_size);
}

int input_demo_object_trace_write(uint32_t frame, char *error, size_t error_size)
{
	return write_object_state(frame, nullptr, error, error_size);
}

int input_demo_object_trace_boundary(uint32_t frame, const char *phase, char *error, size_t error_size)
{
	return write_object_state(frame, phase, error, error_size);
}
