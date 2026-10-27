#include "robot_generator.h"
#include "preview.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "robot.h"
#include "fuelcen.h"
#include "rng_shared.h"
#include "gameseq.h"
#include "boss_health_shared.h"
#include "game.h"
#include "player.h"
#include "gameseg.h"
#include "wall.h"
#include "collide.h"
#include "fireball.h"
#include "digi.h"
#include "sounds.h"
#include "automap.h"
}

namespace
{
std::uint32_t implicit_flags = 0;
std::uint32_t ready_mask = 0;
std::array<matcen_info, MAX_ROBOT_CENTERS> authored{};
int authored_count = 0;

void normalize(vms_vector &vector)
{
	const vms_vector origin{};
	const fix length = xl_robot_generator_distance(&vector, &origin);
	if (!length) vector = {};
	else {
		vector.x = static_cast<fix>(double(vector.x) / double(length) * 65536.0);
		vector.y = static_cast<fix>(double(vector.y) / double(length) * 65536.0);
		vector.z = static_cast<fix>(double(vector.z) / double(length) * 65536.0);
	}
}

vms_vector random_direction()
{
	vms_vector vector;
	const int axis = d_rand() % 3;
	const auto roll = [] { return 16383 - d_rand(); };
	if (axis == 2) {
		vector.x = roll() | 1;
		vector.y = roll();
		vector.z = roll();
	} else if (axis == 1) {
		vector.y = roll() | 1;
		vector.z = roll();
		vector.x = roll();
	} else {
		vector.z = roll() | 1;
		vector.x = roll();
		vector.y = roll();
	}
	normalize(vector);
	return vector;
}
} // namespace

extern "C" void xl_robot_generator_clear(void)
{
	implicit_flags = 0;
	ready_mask = 0;
	authored_count = 0;
}

extern "C" void xl_robot_generator_load(void)
{
	xl_robot_generator_clear();
	if (Num_robot_centers < 0 || Num_robot_centers > MAX_ROBOT_CENTERS) return;
	authored_count = Num_robot_centers;
	for (int i = 0; i < authored_count; ++i) authored[i] = RobotCenters[i];
	// SetVertigoRobotFlags runs on freshly loaded objects, also before save restore
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if (object.type == OBJ_ROBOT && object.id >= 66 && object.id < 96 &&
		    object.id < N_robot_types && !Robot_info[object.id].boss_flag)
			implicit_flags |= std::uint32_t(1) << (object.id - 64);
	}
}

extern "C" std::uint32_t xl_robot_generator_implicit_flags(void)
{
	return xl_level_extensions_active() ? implicit_flags : 0;
}

extern "C" int xl_robot_generator_has_types(int center)
{
	if (!xl_level_extensions_active() || center < 0 || center >= Num_robot_centers) return 0;
	if (!RobotCenters[center].robot_flags[0] && !RobotCenters[center].robot_flags[1]) ready_mask |= std::uint32_t(1) << center;
	return RobotCenters[center].robot_flags[0] || RobotCenters[center].robot_flags[1] || implicit_flags;
}

extern "C" std::uint32_t xl_robot_generator_ready_mask(void)
{
	return ready_mask;
}
extern "C" int xl_robot_generator_restore_ready(std::uint32_t mask, int apply)
{
	if (Num_robot_centers < 0 || Num_robot_centers > MAX_ROBOT_CENTERS || (mask >> Num_robot_centers)) return 0;
	for (int i = 0; i < Num_robot_centers; ++i)
		if ((mask & (std::uint32_t(1) << i)) && (RobotCenters[i].robot_flags[0] || RobotCenters[i].robot_flags[1])) return 0;
	if (apply) ready_mask = mask;
	return 1;
}

extern "C" int xl_robot_generator_spawn_type(int center)
{
	if (!xl_level_extensions_active() || center < 0 || center >= Num_robot_centers) return -1;
	const auto &source = RobotCenters[center];
	// OperateRobotMaker reads word 2 but does not invoke VertigoObjFlags itself
	if (!source.robot_flags[0] && !source.robot_flags[1] && !(ready_mask & (std::uint32_t(1) << center))) return -1;
	return xl_robot_generator_type(center);
}

extern "C" int xl_robot_generator_type(int center)
{
	if (!xl_robot_generator_has_types(center)) return -1;
	const auto &source = RobotCenters[center];
	// The reference's short-circuit refreshes word 2 only when both disk words are zero
	const std::uint32_t flags[] = { std::uint32_t(source.robot_flags[0]), std::uint32_t(source.robot_flags[1]),
		                            source.robot_flags[0] || source.robot_flags[1] ? 0 : implicit_flags };
	std::array<int, 96> types{};
	int count = 0;
	for (int id = 0; id < 96; ++id)
		if (flags[id / 32] & (std::uint32_t(1) << (id % 32))) types[count++] = id;
	if (!count) return -1;
	return types[count == 1 ? 0 : (d_rand() * count) / 32768];
}

extern "C" int32_t xl_robot_generator_distance(const vms_vector *a, const vms_vector *b)
{
	// Default XL math format 2 uses double sqrt on fixed-point components
	const auto delta = [](fix a, fix b) { return double(std::int32_t(std::uint32_t(a) - std::uint32_t(b))); };
	const double x = delta(a->x, b->x), y = delta(a->y, b->y), z = delta(a->z, b->z);
	const double magnitude = std::sqrt(x * x + y * y + z * z);
	// Such large distances all select the same capped generation-time branch
	return magnitude >= double(INT32_MAX) ? INT32_MAX : static_cast<std::int32_t>(magnitude);
}

extern "C" int32_t xl_robot_generator_shields(int type)
{
	if (type < 0 || type >= N_robot_types) return 0;
	const auto &robot = Robot_info[type];
	if (robot.thief || robot.companion)
		return d2_thief_or_companion_health_maximum(robot.strength, Current_level_num, Difficulty_level, robot.companion);
	if (robot.boss_flag) return boss_health_maximum_for_difficulty(robot.strength, Difficulty_level);
	return robot.strength;
}

extern "C" int xl_robot_generator_validate(void)
{
	if (!xl_level_extensions_active()) return 1;
	if (Num_robot_centers != authored_count || Num_fuelcenters < 0 || Num_fuelcenters > MAX_NUM_FUELCENS) return 0;
	for (int i = 0; i < authored_count; ++i) {
		const auto &expected = authored[i], &saved = RobotCenters[i];
		if (saved.robot_flags[0] != expected.robot_flags[0] || saved.robot_flags[1] != expected.robot_flags[1] ||
		    saved.segnum != expected.segnum || saved.fuelcen_num != expected.fuelcen_num || matcen_get_station_num(i) != expected.fuelcen_num) return 0;
	}
	return 1;
}

extern "C" int xl_robot_generator_contact(int object_index)
{
	if (!xl_level_extensions_active() || object_index < 0 || object_index > Highest_object_index) return 0;
	auto &actor = Objects[object_index];
	const bool player = actor.type == OBJ_PLAYER;
	if (!player && actor.type != OBJ_ROBOT) return 0;
	if (actor.segnum < 0 || actor.segnum > Highest_segment_index) return 1;
	if (!player && (actor.id >= N_robot_types || !robot_info_is_usable(actor.id))) return 1;
	digi_link_sound_to_pos(player ? SOUND_PLAYER_GOT_HIT : SOUND_ROBOT_HIT, actor.segnum, 0, &actor.pos, 0, F1_0);
	const int clip = player ? VCLIP_PLAYER_HIT : Robot_info[actor.id].exp1_vclip_num;
	if (clip >= 0) object_create_explosion(actor.segnum, &actor.pos, player ? i2f(5) : static_cast<fix>(std::int64_t(actor.size) * 3 / 8), clip);
	if (player && actor.id != Player_num) return 1;
	vms_vector direction{};
	for (int side = 0; side < 6; ++side)
		if (WALL_IS_DOORWAY(&Segments[actor.segnum], side) & WID_FLY_FLAG) {
			compute_center_point_on_side(&direction, &Segments[actor.segnum], side);
			vm_vec_sub(&direction, &direction, &actor.pos);
			normalize(direction);
			if (player) {
				auto random = random_direction();
				direction.x += random.x / 4;
				direction.y += random.y / 4;
				direction.z += random.z / 4;
				normalize(direction);
			}
		}
	if (actor.movement_type == MT_PHYSICS && actor.mtype.phys_info.mass && !(player && &actor == ConsoleObject && Automap_active)) {
		const float scale = 1.0f / (float(actor.mtype.phys_info.mass) / 65536.0f);
		const fix force = player ? 64 : 8;
		auto &velocity = actor.mtype.phys_info.velocity;
		velocity.x += static_cast<fix>(float(direction.x * force) * scale);
		velocity.y += static_cast<fix>(float(direction.y * force) * scale);
		velocity.z += static_cast<fix>(float(direction.z * force) * scale);
	}
	if (player) apply_damage_to_player(&actor, &actor, i2f(4), 0);
	else apply_damage_to_robot(&actor, F1_0, -1);
	return 1;
}
