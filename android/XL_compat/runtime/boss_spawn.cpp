#include "boss_spawn.h"
#include "object_trigger.h"
#include "preview.h"
#include "robot_generator.h"
#include <algorithm>
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
}

namespace
{
struct Boss {
	int authored_owner, owner, signature, id;
	fix interval;
	fix64 last = 0;
};
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
	for (auto &boss : bosses)
		if (boss.owner == owner && bound(boss)) return &boss;
	return nullptr;
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
	*state = { (std::max) (fix64(0), GameTime64 - boss.last), boss.owner, boss.signature, boss.id, boss.interval };
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
		if (state.owner >= 0 && (state.owner > Highest_object_index || Objects[state.owner].type != OBJ_ROBOT ||
		                         Objects[state.owner].id != state.id || Objects[state.owner].signature != state.signature)) return 0;
	}
	if (apply)
		for (int i = 0; i < count; ++i) {
			bosses[i].owner = states[i].owner;
			bosses[i].signature = states[i].signature;
			bosses[i].interval = states[i].interval;
			bosses[i].last = GameTime64 - states[i].elapsed;
		}
	return 1;
}
extern "C" void xl_boss_spawn_allow_second(int owner)
{
	if (auto *boss = find(owner)) boss->last = GameTime64 - gate_interval() - 1;
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
