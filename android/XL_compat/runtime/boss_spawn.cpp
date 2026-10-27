#include "boss_spawn.h"
#include "boss_routes.h"
#include "boss_destinations.h"
#include "object_trigger.h"
#include "preview.h"
#include "robot_generator.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "robot.h"
#include "polyobj.h"
#include "segment.h"
#include "gameseg.h"
#include "game.h"
#include "player.h"
#include "ai.h"
#include "fuelcen.h"
#include "morph.h"
#include "fireball.h"
#include "digi.h"
#include "vclip.h"
#include "rng_shared.h"
#include "save_epoch_format.h"
#include "wall.h"
#include "fvi.h"
#include "gameseq.h"
#include "cntrlcen.h"
#include "sounds.h"
#include "piggy.h"
#include "guidebot_extensions.h"
#include "boss_health_shared.h"
#include "weapon.h"
int boss_fits_in_seg(object *boss, int segment);
}

namespace
{
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
struct Boss {
	int authored_owner, owner, signature, id;
	fix interval;
	fix64 last = 0;
	fix64 cloak_start = 0, cloak_end = 0, teleport = 0, hit = 0, death_start = 0;
	int dying = 0, sound = 0;
	int home = -1;
	std::vector<int> teleport_segments{};
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif
std::vector<Boss> bosses;
constexpr int spew[8][3] = {
	{ 38, 40, -1 }, { 37, -1, -1 }, { 43, 57, -1 }, { 26, 27, 58 }, { 59, 58, 54 }, { 60, 61, 54 }, { 69, 29, 24 }, { 72, 60, 73 }
};
constexpr int spew_count[8] = { 2, 1, 2, 3, 3, 3, 3, 3 };
fix gate_interval()
{
	return i2f(4) - Difficulty_level * i2f(2) / 3;
}

bool bound(const Boss &boss)
{
	return boss.owner >= 0 && boss.owner <= Highest_object_index &&
	       Objects[boss.owner].type == OBJ_ROBOT && Objects[boss.owner].id == boss.id &&
	       Objects[boss.owner].signature == boss.signature;
}
Boss *find(int owner)
{
	if (!xl_level_extensions_active()) return nullptr;
	for (auto &boss : bosses)
		if (boss.owner == owner && bound(boss)) return &boss;
	return nullptr;
}
std::vector<int> teleport_segments(int owner)
{
	auto &object = Objects[owner];
	const int home = object.segnum;
	const auto position = object.pos;
	const auto result = xl_compat::boss_destinations(home, Num_segments, [](int segment, int side) { return Segments[segment].children[side]; }, [](int segment, int side) { return (WALL_IS_DOORWAY(&Segments[segment], side) & WID_FLY_FLAG) != 0; }, [](int segment) { return xl_level_segment_group(segment); }, [&](int segment) { return boss_fits_in_seg(&object, segment) != 0; });
	object.pos = position;
	obj_relink(owner, home);
	return result;
}
float health_fraction(const object &object)
{
	const fix maximum = boss_health_maximum_for_difficulty(Robot_info[object.id].strength, Difficulty_level);
	return maximum > 0 ? std::clamp(float(object.shields) / float(maximum), 0.0f, 1.0f) : 1.0f;
}
bool valid_teleport(const vms_vector &position, fix size)
{
	// Retain XL's any-distant-actor predicate, rather than substituting intersection
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if ((object.type == OBJ_ROBOT || object.type == OBJ_PLAYER || object.type == OBJ_GHOST || object.type == OBJ_CNTRLCEN || object.type == OBJ_WEAPON) &&
		    xl_robot_generator_distance(&position, &object.pos) > (size + object.size) * 3 / 2) return true;
	}
	return false;
}
void normalize(vms_vector &v)
{
	const vms_vector zero{};
	const fix length = xl_robot_generator_distance(&v, &zero);
	if (!length) v = {};
	else {
		v.x = static_cast<fix>(double(v.x) / double(length) * 65536.0);
		v.y = static_cast<fix>(double(v.y) / double(length) * 65536.0);
		v.z = static_cast<fix>(double(v.z) / double(length) * 65536.0);
	}
}
bool intersects(const vms_vector &position, fix size, int segment)
{
	for (int i = Segments[segment].objects; i != -1; i = Objects[i].next) {
		const auto &object = Objects[i];
		if ((object.type == OBJ_PLAYER || object.type == OBJ_ROBOT || object.type == OBJ_CNTRLCEN) &&
		    xl_robot_generator_distance(&position, &object.pos) < size + object.size) return true;
	}
	return false;
}
vms_vector random_point(int segment)
{
	vms_vector center, delta;
	compute_segment_center(&center, &Segments[segment]);
	// XL Rand(8) uses modulo, unlike Redux's multiply-high vertex selection
	const int vertex = d_rand() % 8;
	vm_vec_sub(&delta, &Vertices[Segments[segment].verts[vertex]], &center);
	vm_vec_scale(&delta, d_rand());
	vm_vec_add2(&center, &delta);
	return center;
}
bool arbitrary_candidate(int id)
{
	return robot_info_is_usable(id) && !Robot_info[id].boss_flag &&
	       !Robot_info[id].companion && Robot_info[id].score_value >= 700;
}
int gated_robot(Boss &boss, int type, int segment, const vms_vector &initial)
{
	if (Game_suspended & SUSP_ROBOTS) return -1;
	boss.interval = gate_interval();
	if (GameTime64 - boss.last < boss.interval) return -1;
	const auto fail = [&] { boss.last = GameTime64 - 3 * boss.interval / 4; return -1; };
	int count = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_ROBOT && Objects[i].matcen_creator == BOSS_GATE_MATCEN_NUM) ++count;
	if (count > 2 * Difficulty_level + 6) return fail();
	const auto &robot = Robot_info[type];
	const fix size = Polygon_models[robot.model_num].rad;
	vms_vector position = initial;
	for (int attempt = 0; intersects(position, size, segment); ++attempt) {
		if (attempt == 4) return fail();
		position = random_point(segment);
	}
	const int index = obj_create(OBJ_ROBOT, type, segment, &position, &vmd_identity_matrix, size, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	if (index < 0) return fail();
	auto &object = Objects[index];
	object.lifeleft = i2f(30) + (F1_0 / 2) * (Difficulty_level * 15);
	object.rtype.pobj_info.model_num = robot.model_num;
	object.rtype.pobj_info.subobj_flags = 0;
	object.mtype.phys_info.mass = robot.mass;
	object.mtype.phys_info.drag = robot.drag;
	object.mtype.phys_info.flags |= PF_LEVELLING;
	object.shields = robot.strength;
	object.matcen_creator = BOSS_GATE_MATCEN_NUM;
	init_ai_object(index, robot.behavior, -1);
	object_create_explosion(segment, &position, i2f(10), VCLIP_MORPHING_ROBOT);
	digi_link_sound_to_pos(Vclip[VCLIP_MORPHING_ROBOT].sound_num, segment, 0, &position, 0, F1_0);
	morph_start(&object);
	boss.last = GameTime64;
	++Players[Player_num].num_robots_level;
	++Players[Player_num].num_robots_total;
	return index;
}
} // namespace

extern "C" void xl_boss_spawn_clear(void)
{
	bosses.clear();
}
extern "C" int xl_boss_spawn_default_type(int flag, int index)
{
	const int slot = flag >= BOSS_D2 ? flag - BOSS_D2 : flag;
	return slot >= 0 && slot < 8 && index >= 0 && index < spew_count[slot] ? spew[slot][index] : -1;
}
extern "C" void xl_boss_spawn_load(void)
{
	bosses.clear();
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if (object.type == OBJ_ROBOT && robot_info_is_usable(object.id) && Robot_info[object.id].boss_flag)
			bosses.push_back({ i, i, object.signature, object.id, gate_interval(), 0 });
	}
	for (auto &boss : bosses) {
		boss.home = Objects[boss.owner].segnum;
		boss.teleport_segments = teleport_segments(boss.owner);
	}
}
extern "C" void xl_boss_object_teleported(int owner)
{
	if (auto *boss = find(owner)) {
		auto segments = teleport_segments(owner);
		boss->home = Objects[owner].segnum;
		boss->teleport_segments.swap(segments);
	}
}

std::vector<xl_compat::BossRoutes> xl_compat::boss_routes()
{
	std::vector<BossRoutes> result;
	if (xl_level_extensions_active())
		for (const auto &boss : bosses) result.push_back({ boss.home, boss.teleport_segments });
	return result;
}

bool xl_compat::restore_boss_routes(std::vector<BossRoutes> &states, int apply)
{
	if (Num_segments < 1 || Num_segments > 9000 || states.size() != static_cast<std::size_t>(xl_boss_spawn_count())) return false;
	for (const auto &state : states) {
		if (state.home < 0 || state.home >= Num_segments || state.segments.empty() ||
		    state.segments.size() > static_cast<std::size_t>((std::min) (1000, Num_segments)) || state.segments.front() != state.home) return false;
		std::array<unsigned char, 9000> seen{};
		for (const int segment : state.segments) {
			if (segment < 0 || segment >= Num_segments || xl_level_segment_group(segment) != xl_level_segment_group(state.home)) return false;
			if (++seen[segment] > (segment == state.home ? 2 : 1)) return false;
		}
	}
	if (apply)
		for (std::size_t i = 0; i < states.size(); ++i) {
			bosses[i].home = states[i].home;
			bosses[i].teleport_segments.swap(states[i].segments);
		}
	return true;
}
extern "C" int xl_boss_spawn_count(void)
{
	return xl_level_extensions_active() ? static_cast<int>(bosses.size()) : 0;
}
extern "C" int xl_boss_spawn_state_get(int index, xl_boss_spawn_state *state)
{
	if (!state || index < 0 || index >= xl_boss_spawn_count()) return 0;
	auto &boss = bosses[index];
	if (!bound(boss)) boss.owner = -1;
	boss.interval = gate_interval();
	*state = { (std::max) (fix64(0), GameTime64 - boss.last), boss.cloak_start - GameTime64, boss.cloak_end - GameTime64, boss.teleport - GameTime64, boss.hit - GameTime64,
		       boss.dying == 1 ? (std::max) (fix64(0), GameTime64 - boss.death_start) : 0, boss.owner, boss.signature, boss.id, boss.interval, boss.dying, boss.sound };
	return 1;
}
extern "C" int xl_boss_spawn_restore(const xl_boss_spawn_state *states, int count, int apply)
{
	if (count != xl_boss_spawn_count() || (!states && count)) return 0;
	for (int i = 0; i < count; ++i) {
		const auto &state = states[i];
		const auto &boss = bosses[i];
		if (state.elapsed < 0 || state.elapsed > SAVE_EPOCH_MAX || state.signature < 0 || state.id != boss.id ||
		    state.interval != gate_interval() || (state.owner != -1 && state.owner != boss.authored_owner)) return 0;
		for (const auto delta : { state.cloak_start, state.cloak_end, state.teleport, state.hit })
			if (delta < -SAVE_EPOCH_MAX || delta > SAVE_EPOCH_MAX) return 0;
		const auto cloak_span = state.cloak_end - state.cloak_start;
		if (cloak_span != 0 && cloak_span != BOSS_CLOAK_DURATION) return 0;
		if (state.dying < 0 || state.dying > 2 || state.sound < 0 || state.sound > 1 || state.death_elapsed < 0 || state.death_elapsed > SAVE_EPOCH_MAX ||
		    (state.dying != 1 && state.death_elapsed) || (!state.dying && state.sound) || (state.dying == 2 && state.owner >= 0)) return 0;
		if (state.owner >= 0 && (state.owner > Highest_object_index || Objects[state.owner].type != OBJ_ROBOT ||
		                         Objects[state.owner].id != state.id || Objects[state.owner].signature != state.signature)) return 0;
		if (state.owner >= 0 && ((state.dying == 1) != (Objects[state.owner].shields < 0))) return 0;
		if (state.owner == -1 && boss.authored_owner <= Highest_object_index) {
			const auto &object = Objects[boss.authored_owner];
			if (object.type == OBJ_ROBOT && object.id == state.id && object.signature == state.signature && !(object.flags & (OF_EXPLODING | OF_SHOULD_BE_DEAD))) return 0;
		}
	}
	if (apply)
		for (int i = 0; i < count; ++i) {
			bosses[i].owner = states[i].owner;
			bosses[i].signature = states[i].signature;
			bosses[i].interval = states[i].interval;
			bosses[i].last = GameTime64 - states[i].elapsed;
			bosses[i].cloak_start = GameTime64 + states[i].cloak_start;
			bosses[i].cloak_end = GameTime64 + states[i].cloak_end;
			bosses[i].teleport = GameTime64 + states[i].teleport;
			bosses[i].hit = GameTime64 + states[i].hit;
			bosses[i].death_start = states[i].dying == 1 ? GameTime64 - states[i].death_elapsed : 0;
			bosses[i].dying = states[i].dying;
			bosses[i].sound = states[i].sound;
		}
	return 1;
}
extern "C" void xl_boss_spawn_allow_second(int owner)
{
	if (auto *boss = find(owner)) boss->last = GameTime64 - gate_interval() - 1;
}
extern "C" void xl_boss_weapon_spawn(int owner, int weapon, const vms_vector *position)
{
	if (!find(owner) || weapon < 0 || weapon > Highest_object_index || !position) return;
	const auto &hit = Objects[weapon];
	if (hit.type != OBJ_WEAPON || hit.id >= N_weapon_types || hit.ctype.laser_info.parent_type != OBJ_PLAYER) return;
	const int slot = Robot_info[Objects[owner].id].boss_flag - BOSS_D2;
	if (slot < 0 || slot >= 8) return;
	constexpr bool energy[] = { true, true, false, true, false, true, true, true };
	constexpr bool kinetic[] = { false, false, true, true, true, true, false, true };
	if (!(Weapon_info[hit.id].matter ? kinetic[slot] : energy[slot])) return;
	if (slot == 1 && d_rand() > 16383 && xl_boss_spawn_robot(owner, -1, position, 0) != -1)
		xl_boss_spawn_allow_second(owner);
	xl_boss_spawn_robot(owner, -1, position, 0);
}
extern "C" int xl_boss_spawn_robot(int owner, int type, const vms_vector *position, int attached)
{
	if (!xl_level_extensions_active() || (Game_mode & GM_MULTI)) return -1;
	auto *boss = find(owner);
	if (!boss || (!attached && xl_object_trigger_has_spawn(owner))) return -1;
	const auto &source = Objects[owner];
	const int segment = position ? find_point_seg(const_cast<vms_vector *>(position), source.segnum) : source.segnum;
	if (segment < 0 || segment > Highest_segment_index) return -1;
	vms_vector origin;
	if (position) origin = *position;
	else compute_segment_center(&origin, &Segments[segment]);
	if (type < 0) {
		const int flag = Robot_info[source.id].boss_flag;
		const int slot = flag >= BOSS_D2 ? flag - BOSS_D2 : flag;
		if (slot < 0 || slot >= 8) return -1;
		type = spew[slot][spew_count[slot] * d_rand() >> 15];
	}
	if (type == 255) {
		bool eligible = false;
		for (int i = 0; i < N_robot_types; ++i) eligible |= arbitrary_candidate(i);
		if (!eligible) return -1;
		do {
			type = d_rand() % N_robot_types;
		} while (!arbitrary_candidate(type));
	}
	if (!robot_info_is_usable(type)) return -1;
	const int index = gated_robot(*boss, type, segment, origin);
	if (index >= 0) {
		auto &object = Objects[index];
		const int force = F1_0 / (FrameTime ? FrameTime : 1);
		if (force) {
			object.ctype.ai_info.SKIP_AI_COUNT += force;
			auto tumble = [&] { return static_cast<fix>(std::int64_t(16383 - d_rand()) * force / 16); };
			object.mtype.phys_info.rotthrust.x = tumble();
			object.mtype.phys_info.rotthrust.y = tumble();
			object.mtype.phys_info.rotthrust.z = tumble();
			object.mtype.phys_info.flags |= PF_USES_THRUST;
			vm_vec_sub(&object.mtype.phys_info.velocity, &origin, &source.pos);
			normalize(object.mtype.phys_info.velocity);
			vm_vec_scale(&object.mtype.phys_info.velocity, i2f(128));
		}
	}
	return index;
}
extern "C" int xl_boss_spawn_action(int owner, int segment)
{
	int type = -1;
	if (segment >= 0 && segment <= Highest_segment_index) type = xl_robot_generator_spawn_type(Segment2s[segment].matcen_num);
	return xl_boss_spawn_robot(owner, type < 0 ? 255 : type, nullptr, 1);
}

extern "C" void xl_boss_note_hit(int owner)
{
	if (auto *boss = find(owner)) boss->hit = GameTime64;
}
extern "C" int xl_boss_cloak_times(int owner, int64_t *start, int64_t *end)
{
	auto *boss = find(owner);
	if (!boss || !start || !end) return 0;
	*start = boss->cloak_start;
	*end = boss->cloak_end;
	return 1;
}
extern "C" int xl_boss_is_dying(int owner)
{
	const auto *boss = find(owner);
	return boss && boss->dying;
}
extern "C" void xl_boss_teleport(int owner)
{
	auto *boss = find(owner);
	if (!boss) return;
	auto &object = Objects[owner];
	if (health_fraction(object) < 0.01 || d_rand() > F1_0 / 2 || boss->teleport_segments.empty() || boss->dying) return;
	int segment = -1;
	vms_vector position;
	for (int attempt = 0; attempt < 5; ++attempt) {
		segment = boss->teleport_segments[d_rand() % boss->teleport_segments.size()];
		compute_segment_center(&position, &Segments[segment]);
		if (valid_teleport(position, object.size)) break;
		segment = -1;
	}
	if (segment < 0) return;
	obj_relink(owner, segment);
	object.pos = position;
	boss->teleport = GameTime64;
	vms_vector forward;
	vm_vec_sub(&forward, &ConsoleObject->pos, &position);
	normalize(forward);
	if (forward.x || forward.y || forward.z) {
		object.orient = {};
		object.orient.fvec = forward;
		if (!forward.x && !forward.z) {
			object.orient.rvec.x = F1_0;
			// Pinned CreateF uses f.z for the vertical special case
			object.orient.uvec.z = forward.z < 0 ? F1_0 : -F1_0;
		} else {
			object.orient.rvec = { forward.z, 0, -forward.x };
			normalize(object.orient.rvec);
			const auto &right = object.orient.rvec;
			object.orient.uvec = {
				static_cast<fix>((double(forward.y) * right.z - double(forward.z) * right.y) / 65536.0),
				static_cast<fix>((double(forward.z) * right.x - double(forward.x) * right.z) / 65536.0),
				static_cast<fix>((double(forward.x) * right.y - double(forward.y) * right.x) / 65536.0)
			};
		}
	}
	escort_note_boss_teleported(owner);
	digi_link_sound_to_pos(Vclip[VCLIP_MORPHING_ROBOT].sound_num, segment, 0, &object.pos, 0, F1_0);
	digi_kill_sound_linked_to_object(owner);
	digi_link_sound_to_object2(Robot_info[object.id].see_sound, owner, 1, F1_0, i2f(512));
	Ai_local_info[owner].next_fire = Ai_local_info[owner].next_fire2 = 0;
}
extern "C" void xl_boss_ai_frame(int owner, int visible)
{
	auto *boss = find(owner);
	if (!boss) return;
	if (boss->last > GameTime64) boss->last = GameTime64;
	if (boss->teleport > GameTime64) boss->teleport = GameTime64;
	if (!visible && GameTime64 - boss->hit > i2f(2)) return;
	auto &object = Objects[owner];
	const fix teleport_interval = Current_level_num == Last_level ? i2f(10) : i2f(7);
	const fix cloak_interval = Current_level_num == Last_level ? i2f(15) : i2f(10);
	if (object.ctype.ai_info.CLOAKED == 1) {
		boss->hit = GameTime64;
		if (GameTime64 - boss->cloak_start > BOSS_CLOAK_DURATION / 3 && boss->cloak_end - GameTime64 > BOSS_CLOAK_DURATION / 3 && GameTime64 - boss->teleport > teleport_interval)
			xl_boss_teleport(owner);
		else if (GameTime64 - boss->hit > i2f(2)) boss->teleport -= teleport_interval / 4;
		if (GameTime64 > boss->cloak_end || GameTime64 < boss->cloak_start || health_fraction(object) < 0.01 || d_rand() > F1_0 / 2)
			object.ctype.ai_info.CLOAKED = 0;
	} else if ((GameTime64 - boss->cloak_end > cloak_interval || GameTime64 - boss->cloak_end < -BOSS_CLOAK_DURATION) && health_fraction(object) >= 0.01 && d_rand() <= F1_0 / 2) {
		boss->cloak_start = GameTime64;
		boss->cloak_end = GameTime64 + BOSS_CLOAK_DURATION;
		object.ctype.ai_info.CLOAKED = 1;
	}
}
extern "C" void xl_boss_start_death(int owner)
{
	if (auto *boss = find(owner); boss && !boss->dying) {
		boss->dying = 1;
		boss->death_start = GameTime64;
	}
}
extern "C" void xl_boss_deaths_frame(void)
{
	if (!xl_level_extensions_active()) return;
	for (auto &boss : bosses) {
		if (!bound(boss) || boss.dying != 1) continue;
		auto &object = Objects[boss.owner];
		const fix elapsed = static_cast<fix>(GameTime64 - boss.death_start);
		object.mtype.phys_info.rotvel = { elapsed / 9, elapsed / 5, elapsed / 7 };
		const int sound = Robot_info[object.id].deathroll_sound;
		const int translated = digi_xlat_sound(sound);
		const fix duration = translated >= 0 ? piggy_sound_duration(translated) : F1_0;
		if (boss.death_start + BOSS_DEATH_DURATION - (duration ? duration : F1_0) < GameTime64) {
			if (!boss.sound) {
				boss.sound = 1;
				digi_link_sound_to_object2(sound, boss.owner, 0, i2f(4), i2f(1024));
			} else if (d_rand() < FrameTime * 16) {
				const fix scale = (F1_0 + d_rand()) * 8;
				const int audible = d_rand() < FrameTime * 2;
				create_small_fireball_on_object(&object, scale, audible);
			}
		} else if (d_rand() < FrameTime * 8) {
			const fix scale = (F1_0 / 2 + d_rand()) * 8;
			const int audible = d_rand() < FrameTime;
			create_small_fireball_on_object(&object, scale, audible);
		}
		if (GameTime64 - boss.death_start > BOSS_DEATH_DURATION) {
			const int owner = boss.owner;
			boss.dying = 2;
			boss.owner = -1;
			do_controlcen_destroyed_stuff(nullptr);
			digi_link_sound_to_object2(SOUND_BADASS_EXPLOSION, owner, 0, i2f(4), i2f(512));
			explode_object(&object, F1_0 / 4);
		}
	}
}
