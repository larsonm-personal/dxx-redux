// Developer-only ordinary-player simulation; initial level and local poses are setup
#include "texture_pixels.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
extern "C" {
#include "collide.h"
#include "physics.h"
}
#include <limits>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
namespace pumo_player_probe
{
using Json = nlohmann::json;
static std::array<unsigned, DXX_XL_MAX_TRIGGERS> operation_counts{};
static unsigned operation_sequence = 0;
static int operation_count_level = 0, operation_frame_source = -1;
static Json *operation_last = nullptr;
static std::ofstream *operation_trace = nullptr;
struct TrackedCombatObject {
	int index, signature, type, id, level;
	unsigned load_generation;
};
static std::vector<TrackedCombatObject> tracked_combat_objects;
static unsigned combat_load_generation = 0;
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
static std::array<xl_player_fvi_sample, 4> player_fvi_first{}, player_fvi_last{};
static unsigned player_fvi_count = 0;
static void player_fvi_observed(const xl_player_fvi_sample *sample)
{
	if (sample->start_segment != 450 || sample->player_object != Players[Player_num].objnum ||
	    !ConsoleObject || sample->player_signature != ConsoleObject->signature) return;
	if (player_fvi_count < player_fvi_first.size()) player_fvi_first[player_fvi_count] = *sample;
	player_fvi_last[player_fvi_count % player_fvi_last.size()] = *sample;
	++player_fvi_count;
}
static Json player_fvi_value(const xl_player_fvi_sample &s)
{
	return { { "level", s.level }, { "player_object", s.player_object }, { "player_signature", s.player_signature }, { "game_time_raw", s.game_time }, { "start_segment", s.start_segment }, { "start_fixed", { s.start[0], s.start[1], s.start[2] } }, { "end_fixed", { s.end[0], s.end[1], s.end[2] } }, { "radius_raw", s.radius }, { "flags", s.flags }, { "fate", s.fate }, { "hit_segment", s.hit_segment }, { "hit_side_segment", s.hit_side_segment }, { "hit_side", s.hit_side }, { "hit_object", s.hit_object }, { "hit_point_fixed", { s.hit_point[0], s.hit_point[1], s.hit_point[2] } }, { "hit_object_type", s.hit_object_type }, { "hit_object_id", s.hit_object_id }, { "hit_object_signature", s.hit_object_signature }, { "robot_energy_drain", s.robot_energy_drain }, { "robot_attack_type", s.robot_attack_type }, { "hit_wall", s.hit_wall }, { "hit_wall_type", s.hit_wall_type }, { "hit_doorway", s.hit_doorway } };
}
static Json player_fvi_result()
{
	Json first = Json::array(), last = Json::array();
	for (unsigned index = 0; index < std::min(player_fvi_count, unsigned(player_fvi_first.size())); ++index)
		first.push_back(player_fvi_value(player_fvi_first[index]));
	if (player_fvi_count > player_fvi_first.size()) {
		const unsigned retained = std::min(player_fvi_count, unsigned(player_fvi_last.size()));
		for (unsigned offset = 0; offset < retained; ++offset)
			last.push_back(player_fvi_value(player_fvi_last[(player_fvi_count - retained + offset) % player_fvi_last.size()]));
	}
	return { { "kind", "actual_local_player_physics_fvi_source450" }, { "sample_count", player_fvi_count }, { "first_limit", player_fvi_first.size() }, { "last_limit", player_fvi_last.size() }, { "first", std::move(first) }, { "last", std::move(last) } };
}
static std::array<xl_weapon_wall_contact, 2> weapon_wall_contacts{};
static unsigned weapon_wall_contact_count = 0;
static bool weapon_wall_pixels_used = false;
static void weapon_wall_contact_observed(const xl_weapon_wall_contact *contact)
{
	if (weapon_wall_contact_count < weapon_wall_contacts.size()) weapon_wall_contacts[weapon_wall_contact_count] = *contact;
	if (weapon_wall_contact_count <= weapon_wall_contacts.size()) ++weapon_wall_contact_count;
}
static Json weapon_wall_contact_result()
{
	Json contacts = Json::array();
	for (unsigned index = 0; index < std::min(weapon_wall_contact_count, unsigned(weapon_wall_contacts.size())); ++index) {
		const auto &c = weapon_wall_contacts[index];
		contacts.push_back({ { "level", c.level }, { "object", c.object }, { "signature", c.signature }, { "weapon_id", c.weapon_id }, { "parent_object", c.parent_object }, { "parent_signature", c.parent_signature }, { "hit_segment", c.hit_segment }, { "hit_side", c.hit_side }, { "hit_point_fixed", { c.point[0], c.point[1], c.point[2] } }, { "wall", c.wall }, { "trigger", c.trigger }, { "base", c.base }, { "overlay_raw", c.overlay }, { "eclip", c.eclip }, { "destroyed", c.destroyed }, { "effect_dest", c.effect_dest }, { "effect_flags", c.effect_flags }, { "blew_up", c.blew_up } });
	}
	return { { "kind", "actual_contacts_reaching_effect_blowup_gate" }, { "contact_count_lower_bound", weapon_wall_contact_count }, { "contact_limit", weapon_wall_contacts.size() }, { "truncated", weapon_wall_contact_count > weapon_wall_contacts.size() }, { "contacts", std::move(contacts) } };
}
static Json diagnostic_weapon_wall_contact_pixels()
{
	Json pixels = Json::array();
	for (unsigned index = 0; index < std::min(weapon_wall_contact_count, unsigned(weapon_wall_contacts.size())); ++index) {
		const auto &contact = weapon_wall_contacts[index];
		require(contact.level == Current_level_num && contact.hit_segment >= 0 && contact.hit_segment <= Highest_segment_index && contact.hit_side >= 0 && contact.hit_side < 6, "native wall pixel contact no longer belongs to this level");
		auto &room = Segments[contact.hit_segment];
		require(room.sides[contact.hit_side].wall_num == contact.wall, "native wall pixel contact wall changed");
		vms_vector point{ contact.point[0], contact.point[1], contact.point[2] };
		fix u = 0, v = 0;
		find_hitpoint_uv(&u, &v, nullptr, &point, &room, contact.hit_side, 0);
		const int overlay_texture = contact.overlay & 0x3fff;
		const int orientation = (contact.overlay >> 14) & 3;
		require(overlay_texture > 0 && overlay_texture < NumTextures && contact.base >= 0 && contact.base < NumTextures, "native wall pixel contact texture out of range");
		const int overlay_transparency = xl_texture_pixel_transparency(overlay_texture, orientation, u, v);
		const int composed_transparency = xl_side_pixel_transparency(contact.base, contact.overlay, u, v);
		const int bitmap_index = Textures[overlay_texture].index;
		require(bitmap_index >= 0 && bitmap_index < Num_bitmap_files, "native wall pixel contact bitmap out of range");
		const auto &bitmap = GameBitmaps[bitmap_index];
		pixels.push_back({ { "level", contact.level }, { "object", contact.object }, { "signature", contact.signature }, { "weapon_id", contact.weapon_id }, { "hit_segment", contact.hit_segment }, { "hit_side", contact.hit_side }, { "wall", contact.wall }, { "trigger", contact.trigger }, { "base", contact.base }, { "overlay_raw", contact.overlay }, { "overlay_texture", overlay_texture }, { "overlay_orientation", orientation }, { "u_raw", int(u) }, { "v_raw", int(v) }, { "overlay_pixel_transparency", overlay_transparency }, { "composed_pixel_transparency", composed_transparency }, { "overlay_bitmap", { { "bitmap_index", bitmap_index }, { "type", int(bitmap.bm_type) }, { "width", int(bitmap.bm_w) }, { "height", int(bitmap.bm_h) }, { "rowsize", int(bitmap.bm_rowsize) }, { "flags", int(bitmap.bm_flags) } } }, { "blew_up", contact.blew_up } });
	}
	return { { "kind", "post_contact_pixel_query_not_collision_qualification" }, { "contact_limit", weapon_wall_contacts.size() }, { "contact_count_lower_bound", weapon_wall_contact_count }, { "truncated", weapon_wall_contact_count > weapon_wall_contacts.size() }, { "pixels", std::move(pixels) } };
}
#endif
static bool combat_object_matches(const TrackedCombatObject &tracked)
{
	if (tracked.level != Current_level_num || tracked.load_generation != combat_load_generation || tracked.index < 0 || tracked.index > Highest_object_index) return false;
	const auto &object = Objects[tracked.index];
	return object.signature == tracked.signature && object.type == tracked.type && object.id == tracked.id;
}
static Json combat_object_state(const TrackedCombatObject &tracked)
{
	const bool exists = combat_object_matches(tracked);
	Json result = { { "object", tracked.index }, { "expected_signature", tracked.signature }, { "expected_type", tracked.type }, { "expected_id", tracked.id }, { "expected_level", tracked.level }, { "expected_load_generation", tracked.load_generation }, { "observed_level", Current_level_num }, { "observed_load_generation", combat_load_generation }, { "exists", exists }, { "actor_state", nullptr }, { "current_slot", nullptr } };
	if (tracked.index < 0 || tracked.index > Highest_object_index) return result;
	const auto &object = Objects[tracked.index];
	result["current_slot"] = { { "signature", int(object.signature) }, { "type", int(object.type) }, { "id", int(object.id) } };
	if (!exists) return result;
	const auto velocity = object.movement_type == MT_PHYSICS ? Json{ int(object.mtype.phys_info.velocity.x), int(object.mtype.phys_info.velocity.y), int(object.mtype.phys_info.velocity.z) } : Json{};
	result["actor_state"] = { { "segment", int(object.segnum) }, { "shields_raw", int(object.shields) }, { "flags", int(object.flags) }, { "should_be_dead", bool(object.flags & OF_SHOULD_BE_DEAD) }, { "movement_type", int(object.movement_type) }, { "position_fixed", { int(object.pos.x), int(object.pos.y), int(object.pos.z) } }, { "velocity_fixed", velocity } };
	return result;
}
static std::string file_hash(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	const std::vector<unsigned char> bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	return picosha2::hash256_hex_string(bytes);
}
static Json snapshot()
{
	// Copy packed mutable fields before JSON forwarding can bind references to them
	Json state = { { "current_level_num", Current_level_num }, { "player_dead", Player_is_dead != 0 }, { "cheats_enabled", cheats.enabled != 0 } };
	state["game_time_raw"] = GameTime64;
	unsigned int rng_state = 0;
	const bool rng_available = d_rand_get_state(&rng_state) != 0;
	state["simulation_rng"] = { { "state_available", rng_available }, { "state", rng_available ? Json(rng_state) : Json{} }, { "call_count", d_rand_get_call_count() } };
	state["level_scene"]["powerups"] = Json::array();
	for (int index = 0; index <= Highest_object_index; ++index)
		if (Objects[index].type == OBJ_POWERUP && !(Objects[index].flags & OF_SHOULD_BE_DEAD))
			state["level_scene"]["powerups"].push_back({ { "object", index }, { "id", Objects[index].id }, { "segment", int(Objects[index].segnum) } });
	const auto &player = Players[Player_num];
	if (Player_is_dead && player.killer_objnum >= 0 && player.killer_objnum <= Highest_object_index) {
		const auto &killer = Objects[player.killer_objnum];
		state["player_death_source"] = { { "object", player.killer_objnum }, { "type", killer.type }, { "id", killer.id }, { "segment", killer.segnum } };
	}
	state["player"] = { { "has_blue_key", bool(player.flags & PLAYER_FLAGS_BLUE_KEY) }, { "has_red_key", bool(player.flags & PLAYER_FLAGS_RED_KEY) }, { "has_gold_key", bool(player.flags & PLAYER_FLAGS_GOLD_KEY) }, { "invulnerable", bool(player.flags & PLAYER_FLAGS_INVULNERABLE) }, { "shields", double(player.shields) / F1_0 }, { "energy", double(player.energy) / F1_0 }, { "score", player.score }, { "lives", player.lives }, { "robots_killed_level", player.num_kills_level } };
	state["player"]["primary_weapon"] = player.primary_weapon;
	state["player"]["primary_weapon_flags"] = player.primary_weapon_flags;
	state["player"]["laser_level"] = int(player.laser_level);
	state["player"]["quad_lasers"] = bool(player.flags & PLAYER_FLAGS_QUAD_LASERS);
	state["player"]["vulcan_ammo"] = player.primary_ammo[VULCAN_INDEX];
	state["player"]["cloaked"] = (player.flags & PLAYER_FLAGS_CLOAKED) != 0;
	state["primary_fire"] = { { "next_delta_raw", Next_laser_fire_time - GameTime64 }, { "count", Global_laser_firing_count }, { "held", Controls.fire_primary_state } };
	state["primary_fire"]["fusion_charge_raw"] = int(Fusion_charge);
	state["primary_fire"]["fusion_auto_fire_pending"] = Auto_fire_fusion_cannon_time != 0;
	state["primary_fire"]["fusion_auto_fire_delta_raw"] = Auto_fire_fusion_cannon_time != 0 ? Json(Auto_fire_fusion_cannon_time - GameTime64) : Json{};
	if (ConsoleObject && Player_ship)
		for (int gun : { 0, 1, 6 }) {
			vms_vector point = Player_ship->gun_points[gun];
			const bool owned = xl_model_gun_point(ConsoleObject->rtype.pobj_info.model_num, gun, &point) != 0;
			state["player"]["muzzles"][std::to_string(gun)] = { { "model", int(ConsoleObject->rtype.pobj_info.model_num) }, { "owned", owned }, { "point_fixed", { int(point.x), int(point.y), int(point.z) } }, { "quick_distance", vm_vec_mag_quick(&point) }, { "legacy_limit_exceeded", vm_vec_mag_quick(&point) > 0x50000 } };
		}
	if (ConsoleObject) state["position"] = { { "segment", int(ConsoleObject->segnum) }, { "x", double(ConsoleObject->pos.x) / F1_0 }, { "y", double(ConsoleObject->pos.y) / F1_0 }, { "z", double(ConsoleObject->pos.z) / F1_0 }, { "radius_raw", int(ConsoleObject->size) }, { "orient_right_fixed", { int(ConsoleObject->orient.rvec.x), int(ConsoleObject->orient.rvec.y), int(ConsoleObject->orient.rvec.z) } }, { "orient_up_fixed", { int(ConsoleObject->orient.uvec.x), int(ConsoleObject->orient.uvec.y), int(ConsoleObject->orient.uvec.z) } }, { "orient_forward_fixed", { int(ConsoleObject->orient.fvec.x), int(ConsoleObject->orient.fvec.y), int(ConsoleObject->orient.fvec.z) } } };
	state["reactor_countdown"] = { { "active", Control_center_destroyed != 0 }, { "total_seconds", Total_countdown_time }, { "timer", Countdown_timer } };
	auto &pilot = state["xl_pilot"];
	pilot["remaining_objectives"] = xl_level_objectives_remaining();
	pilot["boost_player_locked"] = xl_speed_boost_player_locked() != 0;
	pilot["runtime_active"] = xl_level_extensions_active() != 0;
	for (int index = 0; index < Num_triggers; ++index) {
		xl_trigger_state value{};
		pilot["trigger_activated"][std::to_string(index)] = xl_preview_trigger_activated(index) != 0;
		if (xl_wall_trigger_state(index, &value)) pilot["wall_triggers"][std::to_string(index)] = {
			{ "operated", value.operated != 0 }, { "actor", value.object }, { "shot", value.shot }, { "value", int(Triggers[index].value) }, { "type", int(Triggers[index].type) }, { "flags", int(Triggers[index].flags) }, { "delay_ms", value.delay_ms }, { "attached", value.attached != 0 }
		};
		if (operation_counts[index]) pilot["operation_counts"][std::to_string(index)] = operation_counts[index];
	}
	pilot["operation_sequence"] = operation_sequence;
	for (int index = 0; index < Num_walls; ++index) pilot["doorways"][std::to_string(index)] = { { "state", Walls[index].state }, { "type", Walls[index].type }, { "flags", Walls[index].flags }, { "keys", Walls[index].keys }, { "clip", Walls[index].clip_num } };
	pilot["spawn_count"] = xl_spawn_state_count();
	for (int index = 0; index < xl_spawn_state_count(); ++index) {
		xl_spawn_point point{};
		if (!xl_spawn_state_get(index, &point)) continue;
		Json orientation = Json::array();
		for (const auto coordinate : point.orientation) orientation.push_back(coordinate);
		pilot["spawn_points"][std::to_string(index)] = { { "segment", point.segment }, { "side", point.side }, { "position_fixed", { point.position[0], point.position[1], point.position[2] } }, { "orientation_fixed", orientation } };
	}
	for (int index = 0; index < xl_speed_boost_state_count(); ++index) {
		xl_speed_boost_state value{};
		if (xl_speed_boost_state_get(index, &value)) pilot["boosts"][std::to_string(index)] = { { "owner", value.owner } };
	}
	state["level_scene"]["reactors"] = Json::array();
	state["level_scene"]["robots"] = Json::array();
	state["combat_observation"]["load_generation"] = combat_load_generation;
	state["combat_observation"]["binding_policy"] = "invalidate_on_load_or_level_transition";
	state["combat_observation"]["player_object"] = player.objnum;
	state["combat_observation"]["player_signature"] = ConsoleObject ? Json(int(ConsoleObject->signature)) : Json{};
	state["combat_observation"]["tracked_objects"] = Json::array();
	for (const auto &tracked : tracked_combat_objects)
		state["combat_observation"]["tracked_objects"].push_back(combat_object_state(tracked));
	// A before/after snapshot can prove a live emitted shot, not a count of shots
	// that may already have collided or expired between observations.
	constexpr int max_owned_weapons = 64;
	int owned_weapon_count = 0;
	state["combat_observation"]["owned_weapons"] = Json::array();
	for (int index = 0; index <= Highest_object_index; ++index) {
		const auto &object = Objects[index];
		if (object.type != OBJ_WEAPON || (object.flags & OF_SHOULD_BE_DEAD)) continue;
		const auto &laser = object.ctype.laser_info;
		const int parent_type = laser.parent_type, parent_num = laser.parent_num, parent_signature = laser.parent_signature;
		if (parent_type != OBJ_PLAYER || parent_num != player.objnum || !ConsoleObject || parent_signature != ConsoleObject->signature) continue;
		++owned_weapon_count;
		if (owned_weapon_count > max_owned_weapons) continue;
		const auto velocity = object.movement_type == MT_PHYSICS ? Json{ int(object.mtype.phys_info.velocity.x), int(object.mtype.phys_info.velocity.y), int(object.mtype.phys_info.velocity.z) } : Json{};
		state["combat_observation"]["owned_weapons"].push_back({ { "object", index }, { "signature", int(object.signature) }, { "weapon_id", int(object.id) }, { "parent_type", parent_type }, { "parent_object", parent_num }, { "parent_signature", parent_signature }, { "segment", int(object.segnum) }, { "creation_time_raw", int64_t(laser.creation_time) }, { "creation_delta_raw", int64_t(laser.creation_time - GameTime64) }, { "lifeleft_raw", int(object.lifeleft) }, { "position_fixed", { int(object.pos.x), int(object.pos.y), int(object.pos.z) } }, { "radius_raw", int(object.size) }, { "velocity_fixed", velocity } });
	}
	state["combat_observation"]["owned_weapon_count"] = owned_weapon_count;
	state["combat_observation"]["owned_weapons_truncated"] = owned_weapon_count > max_owned_weapons;
	state["combat_observation"]["owned_weapon_limit"] = max_owned_weapons;
	for (int index = 0; index <= Highest_object_index; ++index)
		if (Objects[index].type == OBJ_CNTRLCEN) {
			Json guns = Json::array();
			const auto &object = Objects[index];
			for (int gun = 0; gun < get_reactor_definition(object.id)->n_guns; ++gun) {
				const auto &point = object.ctype.reactor_info.gun_pos[gun], &direction = object.ctype.reactor_info.gun_dir[gun];
				guns.push_back({ { "position_fixed", { point.x, point.y, point.z } }, { "direction_fixed", { direction.x, direction.y, direction.z } } });
			}
			state["level_scene"]["reactors"].push_back({ { "object", index }, { "shields", double(object.shields) / F1_0 }, { "guns", guns } });
		} else if (Objects[index].type == OBJ_ROBOT && ConsoleObject && vm_vec_dist(&Objects[index].pos, &ConsoleObject->pos) < i2f(200))
			state["level_scene"]["robots"].push_back({ { "object", index }, { "id", Objects[index].id }, { "segment", int(Objects[index].segnum) }, { "shields", double(Objects[index].shields) / F1_0 }, { "boss", Robot_info[Objects[index].id].boss_flag }, { "position_fixed", { int(Objects[index].pos.x), int(Objects[index].pos.y), int(Objects[index].pos.z) } } });
	const int primary_id = player.primary_weapon >= 0 && player.primary_weapon < MAX_PRIMARY_WEAPONS ? Primary_weapon_to_weapon_info[player.primary_weapon] : -1;
	for (int weapon : { 0, CONTROLCEN_WEAPON_NUM, primary_id })
		if (weapon >= 0 && weapon < N_weapon_types) state["weapon_definitions"][std::to_string(weapon)] = { { "damage_radius", int(Weapon_info[weapon].damage_radius) }, { "strength", int(Weapon_info[weapon].strength[Difficulty_level]) }, { "speed", int(Weapon_info[weapon].speed[Difficulty_level]) }, { "fire_wait", int(Weapon_info[weapon].fire_wait) }, { "fire_count", int(Weapon_info[weapon].fire_count) }, { "ammo_usage", int(Weapon_info[weapon].ammo_usage) }, { "energy_usage", int(Weapon_info[weapon].energy_usage) }, { "matter", int(Weapon_info[weapon].matter) }, { "blob_size", int(Weapon_info[weapon].blob_size) }, { "render_type", int(Weapon_info[weapon].render_type) }, { "model_num", Weapon_info[weapon].render_type == WEAPON_RENDER_POLYMODEL ? Json(int(Weapon_info[weapon].model_num)) : Json{} } };
	return state;
}
// A bounded static query from live owned bolts, not a gameplay collision qualification
static Json diagnostic_weapon_rays()
{
	constexpr int max_rays = 2;
	constexpr fix sweep_time = F1_0 / 4;
	int owned_weapon_count = 0;
	const int player_object = Players[Player_num].objnum;
	Json rays = Json::array();
	for (int index = 0; index <= Highest_object_index; ++index) {
		const auto &weapon = Objects[index];
		if (weapon.type != OBJ_WEAPON || (weapon.flags & OF_SHOULD_BE_DEAD) || !ConsoleObject) continue;
		const auto &parent = weapon.ctype.laser_info;
		if (parent.parent_type != OBJ_PLAYER || parent.parent_num != player_object || parent.parent_signature != ConsoleObject->signature) continue;
		++owned_weapon_count;
		if (owned_weapon_count > max_rays) continue;
		require(weapon.movement_type == MT_PHYSICS, "owned weapon ray requires a physics weapon");
		require(weapon.segnum >= 0 && weapon.segnum <= Highest_segment_index, "owned weapon ray has invalid segment");
		vms_vector start = weapon.pos;
		const vms_vector velocity = weapon.mtype.phys_info.velocity;
		const auto endpoint = [](fix position, fix speed) {
			const fix64 wide_delta = fixmul64(speed, sweep_time);
			require(wide_delta >= std::numeric_limits<fix>::min() && wide_delta <= std::numeric_limits<fix>::max(), "owned weapon ray displacement overflows fixed coordinates");
			const fix delta = fixmul(speed, sweep_time);
			const int64_t value = int64_t(position) + delta;
			require(value >= std::numeric_limits<fix>::min() && value <= std::numeric_limits<fix>::max(), "owned weapon ray endpoint overflows fixed coordinates");
			return fix(value);
		};
		vms_vector end{ endpoint(start.x, velocity.x), endpoint(start.y, velocity.y), endpoint(start.z, velocity.z) };
		int ignored[] = { -1 };
		fvi_query query{};
		query.p0 = &start;
		query.p1 = &end;
		query.startseg = weapon.segnum;
		query.rad = weapon.size;
		query.thisobjnum = index;
		query.ignore_obj_list = ignored;
		query.flags = FQ_CHECK_OBJS | FQ_TRANSPOINT;
		fvi_info hit{};
		find_vector_intersection(&query, &hit);
		Json ray = { { "object", index }, { "signature", int(weapon.signature) }, { "weapon_id", int(weapon.id) }, { "parent_object", player_object }, { "parent_signature", int(parent.parent_signature) }, { "start_segment", int(weapon.segnum) }, { "start_fixed", { int(start.x), int(start.y), int(start.z) } }, { "end_fixed", { int(end.x), int(end.y), int(end.z) } }, { "velocity_fixed", { int(velocity.x), int(velocity.y), int(velocity.z) } }, { "radius_raw", int(weapon.size) }, { "sweep_time_raw", int(sweep_time) }, { "query_flags", query.flags }, { "hit_type", hit.hit_type }, { "hit_point_fixed", { int(hit.hit_pnt.x), int(hit.hit_pnt.y), int(hit.hit_pnt.z) } }, { "hit_segment", hit.hit_seg }, { "hit_side_segment", hit.hit_side_seg }, { "hit_side", hit.hit_side }, { "hit_face", hit.hit_face }, { "hit_object", hit.hit_object } };
		if (hit.hit_type == HIT_WALL && hit.hit_side_seg >= 0 && hit.hit_side_seg <= Highest_segment_index && hit.hit_side >= 0 && hit.hit_side < 6 && hit.hit_face >= 0 && hit.hit_face < 2) {
			const auto &surface = Segments[hit.hit_side_seg].sides[hit.hit_side];
			const int base_texture = surface.tmap_num, overlay_raw = surface.tmap_num2;
			const int overlay_texture = overlay_raw & 0x3fff;
			fix u = 0, v = 0;
			find_hitpoint_uv(&u, &v, nullptr, &hit.hit_pnt, &Segments[hit.hit_side_seg], hit.hit_side, hit.hit_face);
			ray["wall_texture"] = { { "base", base_texture }, { "overlay_raw", overlay_raw }, { "overlay_texture", overlay_texture }, { "overlay_orientation", int((overlay_raw >> 14) & 3) }, { "u_raw", int(u) }, { "v_raw", int(v) }, { "pixel_transparency", xl_side_pixel_transparency(base_texture, overlay_raw, u, v) } };
			for (const auto &[name, texture] : { std::pair<const char *, int>{ "base_bitmap", base_texture }, { "overlay_bitmap", overlay_texture } }) {
				if (texture < 0 || texture >= NumTextures || (name[0] == 'o' && !texture)) continue;
				const int bitmap_index = Textures[texture].index;
				if (bitmap_index < 0 || bitmap_index >= Num_bitmap_files) continue;
				const auto &bitmap = GameBitmaps[bitmap_index];
				ray["wall_texture"][name] = { { "texture", texture }, { "bitmap_index", bitmap_index }, { "type", int(bitmap.bm_type) }, { "width", int(bitmap.bm_w) }, { "height", int(bitmap.bm_h) }, { "rowsize", int(bitmap.bm_rowsize) }, { "flags", int(bitmap.bm_flags) } };
			}
		}
		rays.push_back(std::move(ray));
	}
	return { { "kind", "constant_velocity_static_query_not_collision_qualification" }, { "ray_limit", max_rays }, { "sweep_time_raw", int(sweep_time) }, { "owned_weapon_count", owned_weapon_count }, { "rays_truncated", owned_weapon_count > max_rays }, { "rays", std::move(rays) } };
}
using TriggerObservation = std::vector<std::array<std::int64_t, 11>>;
static TriggerObservation trigger_observation()
{
	TriggerObservation result;
	result.reserve(Num_triggers + 1);
	result.push_back({ Current_level_num, operation_sequence });
	for (int index = 0; index < Num_triggers; ++index) {
		xl_trigger_state state{};
		const bool valid = xl_wall_trigger_state(index, &state) != 0;
		result.push_back({ xl_preview_trigger_activated(index), valid, valid ? state.operated : 0, valid ? state.object : -1, valid ? state.shot : 0, valid ? state.delay_ms : -1, valid ? state.attached : 0, Triggers[index].type, Triggers[index].flags, Triggers[index].value, operation_counts[index] });
	}
	return result;
}
static void operation_observed(int index, int shot, int pre_exit)
{
	if (index < 0 || index >= int(operation_counts.size())) return;
	if (operation_count_level != Current_level_num) {
		operation_counts.fill(0);
		operation_count_level = Current_level_num;
	}
	++operation_counts[index];
	++operation_sequence;
	if (!operation_last || !operation_trace) return;
	auto state = snapshot();
	state["observation_boundary"] = pre_exit ? "pre_exit" : "post_operation";
	// A destination automatic action can run before GameProcessFrame returns
	if (operation_frame_source < 0 || operation_frame_source == Current_level_num) *operation_last = state;
	*operation_trace << Json{ { "event", "successful_wall_operation" }, { "index", index }, { "shot", shot }, { "sequence", operation_sequence }, { "state", state } }.dump() << '\n';
}
static bool matches(const Json &actual, const Json &expected)
{
	if (!expected.is_object()) return actual == expected;
	for (const auto &[key, value] : expected.items()) {
		if (key == "contains_object" || key == "not_contains_object") {
			if (!actual.is_array()) return false;
			const bool found = std::any_of(actual.begin(), actual.end(), [&](const auto &item) { return matches(item, value); });
			if ((key == "contains_object" && !found) || (key == "not_contains_object" && found)) return false;
		} else if (key == "lt" || key == "gt" || key == "lte" || key == "gte") {
			if (!actual.is_number() || !value.is_number()) return false;
			const double a = actual.get<double>(), b = value.get<double>();
			if ((key == "lt" && !(a < b)) || (key == "gt" && !(a > b)) || (key == "lte" && !(a <= b)) || (key == "gte" && !(a >= b))) return false;
		} else if (!actual.is_object() || !actual.contains(key) || !matches(actual.at(key), value)) return false;
	}
	return true;
}
static bool expectations(const Json &state, const Json &expected, std::string &failed)
{
	for (const auto &[path, value] : expected.items()) {
		const Json *actual = &state;
		std::istringstream names(path);
		std::string key;
		while (std::getline(names, key, '.')) {
			if (!actual->is_object() || !actual->contains(key)) {
				failed = path + " missing";
				return false;
			}
			actual = &actual->at(key);
		}
		if (!matches(*actual, value)) {
			failed = path + " actual=" + actual->dump() + " expected=" + value.dump();
			return false;
		}
	}
	return true;
}
static void pose(const Json &step)
{
	const int segment = step.at("segment");
	require(segment >= 0 && segment <= Highest_segment_index, "native player pose segment out of range");
	vms_vector position, forward;
	if (step.at("action") == "face_view") {
		const int side = step.at("side");
		require(side >= 0 && side < 6 && Segments[segment].sides[side].type == SIDE_IS_QUAD, "native player probe requires a quad local approach");
		vms_vector center, inward;
		compute_center_point_on_side(&position, &Segments[segment], side);
		compute_segment_center(&center, &Segments[segment]);
		vm_vec_sub(&forward, &center, &position);
		const fix length = vm_vec_normalize_quick(&forward);
		inward = Segments[segment].sides[side].normals[0];
		if (vm_vec_dot(&inward, &forward) < 0) vm_vec_negate(&inward);
		vm_vec_normalize_quick(&inward);
		fix distance = fl2f(step.value("distance", 8.0));
		if (distance >= length) distance = length / 2;
		require(distance > 0, "native player approach has no clearance");
		vm_vec_scale_add2(&position, &inward, distance);
		forward = inward;
		vm_vec_negate(&forward);
		vm_vector_2_matrix(&ConsoleObject->orient, &forward, nullptr, nullptr);
	} else {
		const auto &point = step.at("position_fixed");
		position = { point[0].get<int>(), point[1].get<int>(), point[2].get<int>() };
		const vms_angvec angles{ static_cast<fixang>(step.value("pitch", 0)), static_cast<fixang>(step.value("bank", 0)), static_cast<fixang>(step.value("heading", 0)) };
		vm_angles_2_matrix(&ConsoleObject->orient, &angles);
	}
	const int found = find_point_seg(&position, segment);
	require(found >= 0, "native player approach is outside the original mine");
	obj_relink(ConsoleObject - Objects, found);
	ConsoleObject->pos = ConsoleObject->last_pos = position;
	ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->mtype.phys_info.thrust = ConsoleObject->mtype.phys_info.rotthrust = {};
}
static int run(int argc, char **argv)
{
	const auto *extra = find_arg_value(argc, argv, "-extra-dir");
	if (!extra || !*extra) {
		std::cerr << "Native Pumo player probe requires an isolated extra directory\n";
		return 1;
	}
	const auto root = std::filesystem::path(extra);
	const auto *resource_argument = find_arg_value(argc, argv, "-pumo-resource-root");
	const auto resources = resource_argument ? std::filesystem::path(resource_argument) : root;
	std::ofstream trace(root / "player-trace.jsonl");
	int completed = 0, view_aim_events = 0, position_assist_events = 0;
	const auto checkpoint_tag = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	bool runtime_initialized = false;
	std::string resume_sha256, run_id, plan_sha256, executable_sha256;
	try {
		tracked_combat_objects.clear();
		combat_load_generation = 0;
		const auto plan_path = std::filesystem::path(find_arg_value(argc, argv, "-pumo-player-test"));
		const auto executable_path = std::filesystem::canonical(argv[0]);
		require(std::filesystem::is_regular_file(plan_path) && std::filesystem::is_regular_file(executable_path) && std::filesystem::file_size(plan_path) > 0 && std::filesystem::file_size(executable_path) > 0, "native player requires readable nonempty plan and executable files");
		plan_sha256 = file_hash(plan_path);
		executable_sha256 = file_hash(executable_path);
		for (const auto &[file, expected] : std::map<std::string, std::string>{
		         { "missions/pmines.hog", "6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043" },
		         { "missions/pmines.mn2", "45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd" },
		         { "mods/pmines/pmines.vham", "a623c7a909e6a7b546816f5082e57d33a19fc4b59f84bb3f8493df3ac86b6b05" } }) {
			require(file_hash(resources / file) == expected, ("Native Pumo player requires original " + file).c_str());
		}
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error), true), error);
		runtime_initialized = true;
		require(mount_extra_dir(root.string().c_str(), error, sizeof(error)), error);
		if (resources != root)
			require(PHYSFS_addToSearchPath(resources.string().c_str(), 1), "cannot mount read-only original Pumo resource input");
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_base_mission("pmines", error, sizeof(error)), error);
		require(N_robot_types == 78 && N_weapon_types == 65 && Robot_info[68].model_num == 17 && xl_owned_oof_model(171), "native player requires full original Pumo definitions/models, not Vertigo");
		const auto &sources = xl_resource_mod_sources();
		for (const auto *file : { "mods/pmines/pmines.ham", "mods/pmines/pmines.hxm", "mods/pmines/pmines.vham", "mods/pmines/models/model171.oof" })
			require(sources.count(file), "native player is missing original mission-owned resources");
		std::ifstream input(plan_path);
		Json plan;
		input >> plan;
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
		const bool observe_weapon_wall_contacts = plan.value("observe_weapon_wall_contacts", false);
		const bool observe_player_fvi = plan.value("observe_player_fvi", false);
		player_fvi_count = 0;
		weapon_wall_contact_count = 0;
		weapon_wall_pixels_used = false;
#endif
		// Legacy direct Majha phases do not pass a nonce; the strict phase owner does
		run_id = plan.value("receipt_run_id", std::string{});
		std::strcpy(Players[Player_num].callsign, "PumoNat");
		Difficulty_level = 0;
		input_demo_set_skip_level_intro(1);
		const int initial_level = plan.at("initial_level");
		require(initial_level >= 1 && initial_level <= Last_level, "native player initial level out of range");
		StartNewGame(initial_level);
		++combat_load_generation;
		if (plan.contains("resume_checkpoint")) {
			const std::string file = plan.at("resume_checkpoint");
			require(!file.empty() && std::filesystem::path(file).filename() == file, "native checkpoint must name an isolated save file");
			require(std::filesystem::is_regular_file(root / "preview-user" / file), "native earned checkpoint file missing");
			resume_sha256 = file_hash(root / "preview-user" / file);
			require(resume_sha256 == plan.at("resume_sha256"), "native earned checkpoint hash changed");
			std::vector<char> name(file.begin(), file.end());
			name.push_back(0);
			require(state_restore_all_sub(name.data(), 0), "native earned checkpoint failed to restore");
			++combat_load_generation;
		}
		if (plan.contains("track_objects")) {
			const auto &targets = plan.at("track_objects");
			require(targets.is_array() && targets.size() <= 16, "native combat tracking requires at most 16 object indices");
			for (const auto &target : targets) {
				require(target.is_number_integer(), "native combat tracking index must be an integer");
				const int index = target.get<int>();
				require(index >= 0 && index <= Highest_object_index && (Objects[index].type == OBJ_ROBOT || Objects[index].type == OBJ_CNTRLCEN) && !(Objects[index].flags & OF_SHOULD_BE_DEAD), "native combat tracking target is not a live combat object");
				require(std::none_of(tracked_combat_objects.begin(), tracked_combat_objects.end(), [index](const auto &entry) { return entry.index == index; }), "native combat tracking contains a duplicate object index");
				const auto &object = Objects[index];
				tracked_combat_objects.push_back({ index, object.signature, object.type, object.id, Current_level_num, combat_load_generation });
			}
		}
		if (plan.contains("resume_checkpoint")) {
			std::string failure;
			const bool restored = expectations(snapshot(), plan.at("checkpoint_expect"), failure);
			require(restored, ("native earned checkpoint: " + failure).c_str());
		}
		double thrust = 0, sideways = 0;
		bool firing = false;
		int remainder = 0, fps = plan.value("simulation_fps", 60), frame_count = 0;
		bool weapon_rays_used = false;
		require(fps >= 1 && fps <= 120, "native player simulation rate out of range");
		operation_counts.fill(0);
		operation_sequence = 0;
		operation_count_level = Current_level_num;
		operation_frame_source = -1;
		Json transitions = Json::array(), checkpoints = Json::object(), checkpoint_history = Json::array(), last_activation = snapshot();
		last_activation["observation_boundary"] = "post_load";
		operation_last = &last_activation;
		operation_trace = &trace;
		xl_wall_trigger_set_operation_observer(operation_observed);
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
		if (observe_weapon_wall_contacts) xl_weapon_wall_set_contact_observer(weapon_wall_contact_observed);
		if (observe_player_fvi) xl_player_fvi_set_observer(player_fvi_observed);
#endif
		auto observed_state = trigger_observation();
		const auto tick = [&] {
			const auto frame_started = std::chrono::steady_clock::now();
			timer_update();
			require(!Player_is_dead && !cheats.enabled && !cheats.ghostphysics && !cheats.monsterdamage && !cheats.robotfiringsuspended && !(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "ordinary player probe died or acquired assisted combat");
			remainder += F1_0;
			FrameTime = remainder / fps;
			remainder %= fps;
			Controls = {};
			Controls.forward_thrust_time = static_cast<fix>(thrust * FrameTime);
			Controls.sideways_thrust_time = static_cast<fix>(sideways * FrameTime);
			Controls.fire_primary_state = firing;
			do_weapon_n_item_stuff();
			calc_game_time();
			// Presentation is outside this simulation probe, including later levels
			input_demo_set_skip_level_intro(1);
			const int before_level = Current_level_num;
			operation_frame_source = before_level;
			GameProcessFrame();
			++frame_count;
			operation_frame_source = -1;
			if (before_level != Current_level_num) ++combat_load_generation;
			// Wall-trigger debounce uses the real timer, not GameTime64
			if (plan.value("realtime", false)) std::this_thread::sleep_until(frame_started + std::chrono::microseconds(1000000 / fps));
			if (operation_count_level != Current_level_num) {
				operation_counts.fill(0);
				operation_count_level = Current_level_num;
			}
			const auto state_now = trigger_observation();
			if (before_level != Current_level_num) {
				transitions.push_back({ { "from", before_level }, { "to", Current_level_num }, { "last_activation", last_activation } });
				const auto post_frame = snapshot();
				trace << Json{ { "event", "normal_level_transition" }, { "transition", transitions.back() }, { "state", post_frame } }.dump() << '\n';
				last_activation = post_frame;
				last_activation["observation_boundary"] = "post_frame";
			} else if (state_now != observed_state) {
				last_activation = snapshot();
				last_activation["observation_boundary"] = "post_frame";
				trace << Json{ { "event", "trigger_change" }, { "state", last_activation } }.dump() << '\n';
			}
			observed_state = state_now;
		};
		for (const auto &step : plan.at("steps")) {
			const std::string action = step.at("action");
			trace << Json{ { "step", completed }, { "action", action }, { "simulation_fps", fps }, { "state", snapshot() } }.dump() << '\n';
			trace.flush();
			std::string failed;
			if (action == "assert") {
				const bool passed = expectations(snapshot(), step.at("expect"), failed);
				require(passed, failed.c_str());
			} else if (action == "face_view" || action == "pose_view") {
				pose(step);
				++position_assist_events;
				trace << Json{ { "event", "position_assist" }, { "action", action }, { "step", completed }, { "position", snapshot().at("position") } }.dump() << '\n';
			}
			else if (action == "send_axis") {
				const int axis = step.at("axis");
				require(axis == 0 || axis == 1, "native player probe supports sideways/forward thrust axes");
				require(step.at("axis_value").get<double>() >= -1 && step.at("axis_value").get<double>() <= 1, "native thrust axis out of range");
				if (axis == 0) sideways = step.at("axis_value").get<double>();
				else thrust = -step.at("axis_value").get<double>();
			} else if (action == "send_button") {
				require(step.at("button") == 100, "native player probe only supports primary fire");
				firing = step.value("held", step.value("pressed", 0)) != 0;
			} else if (action == "select_primary") {
				const int slot = step.at("slot");
				require(slot >= 0 && slot < 5 && !firing, "native weapon selection requires a normal released-fire weapon slot");
				// Use the keyboard selection path, including ownership and upgrade checks
				do_weapon_select(slot, 0);
			} else if (action == "wait_ms") {
				for (int frame = 0, count = (step.at("ms").get<int>() * fps + 999) / 1000; frame < count; ++frame) tick();
			} else if (action == "wait_for") {
				const int limit = (step.value("timeout_ms", 10000) * fps + 999) / 1000;
				const int target = step.value("face_object", -1);
				const int interval = step.value("face_interval_ms", 200);
				require(target == -1 || (target >= 0 && target <= Highest_object_index && (Objects[target].type == OBJ_ROBOT || Objects[target].type == OBJ_CNTRLCEN)), "native view target is not a live combat object");
				const TrackedCombatObject aimed_target = target >= 0 ? TrackedCombatObject{ target, Objects[target].signature, Objects[target].type, Objects[target].id, Current_level_num, combat_load_generation } : TrackedCombatObject{ -1, -1, -1, -1, Current_level_num, combat_load_generation };
				require(interval >= 17 && interval <= 1000, "native view interval out of range");
				const int aim_frames = std::max(1, (interval * fps + 999) / 1000);
				int frame = 0;
				while (!expectations(snapshot(), step.at("expect"), failed) && frame < limit) {
					if (target >= 0 && frame % aim_frames == 0 && combat_object_matches(aimed_target) && !(Objects[target].flags & OF_SHOULD_BE_DEAD) && Objects[target].shields > 0) {
						vms_vector forward;
						vm_vec_sub(&forward, &Objects[target].pos, &ConsoleObject->pos);
						if (vm_vec_normalize_quick(&forward)) vm_vector_2_matrix(&ConsoleObject->orient, &forward, nullptr, nullptr);
						++view_aim_events;
						trace << Json{ { "event", "fixed_view_aim" }, { "target", target }, { "target_state", combat_object_state(aimed_target) }, { "game_time_raw", GameTime64 }, { "forward_fixed", { int(forward.x), int(forward.y), int(forward.z) } } }.dump() << '\n';
					}
					tick();
					++frame;
				}
				const bool passed = expectations(snapshot(), step.at("expect"), failed);
				require(passed, ("native wait: " + failed).c_str());
			} else if (action == "set_debug") {
				require(step.at("field") == "android_game_request", "unsupported native player request");
				char file[] = "pumo-player-quick.sav", description[] = "Ordinary Pumo checkpoint";
				if (step.at("value") == "quick_save") {
					stop_time();
					require(state_save_all_sub(file, description), "native player checkpoint save failed");
					// Keep every same-level checkpoint, plus the existing latest-level alias
					const auto prefix = "pumo-player-level" + std::to_string(Current_level_num);
					const auto retained = prefix + ".sav";
					const auto unique = prefix + "-" + checkpoint_tag + "-step" + std::to_string(completed) + ".sav";
					std::filesystem::copy_file(root / "preview-user" / file, root / "preview-user" / unique);
					std::filesystem::copy_file(root / "preview-user" / unique, root / "preview-user" / retained, std::filesystem::copy_options::overwrite_existing);
					const auto state = snapshot();
					const auto hash = file_hash(root / "preview-user" / unique);
					checkpoint_history.push_back({ { "step", completed }, { "file", unique }, { "sha256", hash }, { "state", state } });
					checkpoints[std::to_string(Current_level_num)] = { { "file", retained }, { "sha256", hash }, { "state", state } };
				} else {
					require(step.at("value") == "quick_load", "unsupported native player save action");
					require(state_restore_all_sub(file, 0), "native player checkpoint load failed");
					// A load invalidates every prior target binding, even if its slot and signature recur
					++combat_load_generation;
					operation_counts.fill(0);
					operation_count_level = Current_level_num;
					last_activation = snapshot();
					last_activation["observation_boundary"] = "post_load";
					observed_state = trigger_observation();
				}
			} else if (action == "diagnostic_weapon_rays") {
				require(frame_count > 0 && !weapon_rays_used, "native weapon rays require a prior ordinary frame and run once");
				require(!input_demo_recorder_is_active() && !input_demo_replay_is_loaded(), "native weapon rays require recorder and replay off");
				weapon_rays_used = true;
				const auto before = snapshot();
				const auto result = diagnostic_weapon_rays();
				const auto after = snapshot();
				trace << Json{ { "event", "diagnostic_weapon_rays" }, { "step", completed }, { "result", result }, { "common_state_unchanged", before == after } }.dump() << '\n';
				require(before == after, "native weapon ray query changed common state or RNG");
			} else if (action == "diagnostic_weapon_wall_contact_pixels") {
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
				require(plan.value("observe_weapon_wall_contacts", false) && frame_count > 0 && weapon_wall_contact_count > 0 && !weapon_wall_pixels_used, "native wall pixels require prior contacts and run once");
				require(!input_demo_recorder_is_active() && !input_demo_replay_is_loaded(), "native wall pixels require recorder and replay off");
				weapon_wall_pixels_used = true;
				const auto before = snapshot();
				const auto result = diagnostic_weapon_wall_contact_pixels();
				const auto after = snapshot();
				trace << Json{ { "event", "diagnostic_weapon_wall_contact_pixels" }, { "step", completed }, { "result", result }, { "common_state_unchanged", before == after } }.dump() << '\n';
				require(before == after, "native wall pixel query changed common state or RNG");
#else
				require(false, "native wall pixel query requires developer metadata target");
#endif
			} else if (action == "frame_rate") {
				fps = step.at("fps");
				require(fps >= 1 && fps <= 120, "native player simulation rate out of range");
				remainder = 0;
			} else require(action == "introspect", "unsupported native player action");
			++completed;
		}
		std::ofstream(root / "player-state.json") << snapshot().dump(2) << '\n';
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
		if (observe_weapon_wall_contacts) trace << Json{ { "event", "weapon_wall_contacts" }, { "result", weapon_wall_contact_result() } }.dump() << '\n';
		if (observe_player_fvi) trace << Json{ { "event", "player_physics_fvi" }, { "result", player_fvi_result() } }.dump() << '\n';
#endif
#ifdef _WIN32
		const int process = _getpid();
#else
		const int process = getpid();
#endif
		std::ofstream(root / "player-result.json") << Json{ { "result", "PASS" }, { "receipt_version", 2 }, { "run_id", run_id }, { "plan_sha256", plan_sha256 }, { "executable_sha256", executable_sha256 }, { "resume_sha256", resume_sha256 }, { "steps", completed }, { "process_id", process }, { "initial_level_setup", initial_level }, { "resumed_earned_checkpoint", plan.contains("resume_checkpoint") }, { "final_level", Current_level_num }, { "transitions", transitions }, { "checkpoints", checkpoints }, { "checkpoint_history", checkpoint_history }, { "rendered", bool{ false } }, { "fixed_view_aim_events", view_aim_events }, { "position_assist_events", position_assist_events }, { "operation_sequence", operation_sequence } }.dump(2) << '\n';
		xl_wall_trigger_set_operation_observer(nullptr);
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
		xl_weapon_wall_set_contact_observer(nullptr);
		xl_player_fvi_set_observer(nullptr);
#endif
		std::cout << "Original Pumo ordinary player controls passed: " << completed << " steps, initial level setup " << initial_level << " -> " << Current_level_num << "; unrendered\n";
		return 0;
	} catch (const std::exception &error) {
		xl_wall_trigger_set_operation_observer(nullptr);
#ifdef DXX_PUMO_CONTACT_DEVELOPER_PROBE
		xl_weapon_wall_set_contact_observer(nullptr);
		xl_player_fvi_set_observer(nullptr);
#endif
		std::ofstream(root / "player-state.json") << (runtime_initialized ? snapshot() : Json::object()).dump(2) << '\n';
		std::ofstream(root / "player-result.json") << Json{ { "result", "FAIL" }, { "steps", completed }, { "error", error.what() }, { "fixed_view_aim_events", view_aim_events }, { "position_assist_events", position_assist_events } }.dump(2) << '\n';
		std::cerr << "Original Pumo player probe failed: " << error.what() << '\n';
		return 1;
	}
}
} // namespace pumo_player_probe
