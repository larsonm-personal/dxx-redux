#include "speed_boost.h"
#include "preview.h"
#include "motion_math.h"
#include <algorithm>
#include <array>
#include <cstdint>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
#include "kconfig.h"
#include "hudmsg.h"
}

namespace
{
std::array<xl_speed_boost_state, MAX_OBJECTS> states{};
int last_speed = 0;
constexpr fix step_time = F1_0 / 64;
fix multiply(fix a, fix b)
{
	return static_cast<fix>(double(b) / 65536.0 * a);
}
fix divide(fix a, fix b)
{
	return b ? static_cast<fix>(double(a) / b * 65536.0) : 1;
}
vms_vector vector(const int32_t value[3])
{
	return { value[0], value[1], value[2] };
}
void store(int32_t target[3], const vms_vector &value)
{
	target[0] = value.x;
	target[1] = value.y;
	target[2] = value.z;
}
vms_vector scale(const vms_vector &value, fix amount)
{
	return { multiply(value.x, amount), multiply(value.y, amount), multiply(value.z, amount) };
}
vms_vector normalize(const vms_vector &value)
{
	const auto result = xl_compat::geometry_fixed_direction({ value.x, value.y, value.z });
	return { result[0], result[1], result[2] };
}
bool live(int index)
{
	return xl_level_extensions_active() && index >= 0 && index <= Highest_object_index &&
	       (Objects[index].type == OBJ_PLAYER || xl_wall_teleport_actor(index));
}
xl_speed_boost_state *state_for(int index)
{
	if (!live(index)) return nullptr;
	auto &state = states[index];
	if (state.signature != Objects[index].signature || state.owner != index) state = {};
	return state.boosted || state.steps ? &state : nullptr;
}
vms_vector side_direction(int segment, int side)
{
	const auto geometry = xl_compat::compile_classic_lighting_side(segment, side);
	return { -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][0]) + geometry.fixed_normals[1][0]) / 2),
		     -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][1]) + geometry.fixed_normals[1][1]) / 2),
		     -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][2]) + geometry.fixed_normals[1][2]) / 2) };
}
vms_angvec matrix_angles(const vms_matrix &matrix)
{
	vms_angvec angles{};
	angles.h = matrix.fvec.x || matrix.fvec.z ? fix_atan2(matrix.fvec.z, matrix.fvec.x) : 0;
	const auto trig = xl_compat::teleport_matrix({ 0, 0, angles.h });
	const fix sine = trig.fvec.x, cosine = trig.fvec.z;
	const fix pitch_cosine = std::abs(sine) > std::abs(cosine) ? divide(matrix.fvec.x, sine) : divide(matrix.fvec.z, cosine);
	angles.p = pitch_cosine || matrix.fvec.y ? fix_atan2(pitch_cosine, -matrix.fvec.y) : 0;
	if (pitch_cosine) {
		const fix bank_sine = divide(matrix.rvec.y, pitch_cosine), bank_cosine = divide(matrix.uvec.y, pitch_cosine);
		angles.b = bank_sine || bank_cosine ? fix_atan2(bank_cosine, bank_sine) : 0;
	}
	return angles;
}
void orient(object &actor, xl_speed_boost_state &state, int step)
{
	const auto direction = vector(state.direction);
	if (xl_compat::teleport_dot(direction, actor.orient.fvec) < 0) {
		vm_vec_negate(&actor.orient.fvec);
		vm_vec_negate(&actor.orient.rvec);
	}
	const auto target = xl_compat::teleport_angles(direction);
	auto velocity = xl_compat::teleport_angles(actor.mtype.phys_info.velocity);
	velocity.p -= target.p;
	velocity.b -= target.b;
	velocity.h -= target.h;
	if (step > 1) {
		velocity.p /= step;
		velocity.b /= step;
		velocity.h /= step;
		auto current = matrix_angles(actor.orient);
		// Angle-vector subtraction wraps to 16 bits before division in XL
		current.p += static_cast<fixang>(target.p - current.p) / step;
		current.b += static_cast<fixang>(target.b - current.b) / step;
		current.h += static_cast<fixang>(target.h - current.h) / step;
		actor.orient = xl_compat::teleport_matrix(current);
	} else actor.orient = xl_compat::teleport_matrix(target);
	const auto rotation = xl_compat::teleport_matrix(velocity);
	const auto previous = actor.mtype.phys_info.velocity;
	actor.mtype.phys_info.velocity = { xl_compat::teleport_dot(previous, rotation.rvec), xl_compat::teleport_dot(previous, rotation.uvec), xl_compat::teleport_dot(previous, rotation.fvec) };
}
void set_velocity(object &actor, xl_speed_boost_state &state, const vms_vector &direction, int speed)
{
	const auto velocity = scale(direction, speed * F1_0);
	store(state.velocity, velocity);
	for (int axis = 0; axis < 3; ++axis) {
		state.minimum[axis] = state.velocity[axis] - 60 * F1_0;
		state.maximum[axis] = state.velocity[axis] + 60 * F1_0;
	}
	actor.mtype.phys_info.velocity = velocity;
}
} // namespace

extern "C" void xl_speed_boost_clear(void)
{
	states.fill({});
}
extern "C" void xl_speed_boost_forget(int index)
{
	if (index >= 0 && index < MAX_OBJECTS) states[index] = {};
}
extern "C" int xl_speed_boost_active(int index)
{
	const auto *state = state_for(index);
	return state ? state->boosted : 0;
}
extern "C" int xl_speed_boost_player_locked(void)
{
	return xl_speed_boost_active(Players[Player_num].objnum) > 0;
}
extern "C" void xl_speed_boost_stop(int index)
{
	if (!live(index)) return;
	states[index] = {};
	if (!last_speed) last_speed = 10;
	auto &velocity = Objects[index].mtype.phys_info.velocity;
	velocity = scale(normalize(velocity), 60 * F1_0);
}

extern "C" int xl_speed_boost_action(const trigger *action, int index, int object_index, int shot)
{
	if (!action || action->type != 15 || !live(object_index)) return 1;
	auto &actor = Objects[object_index];
	if (actor.type == OBJ_PLAYER && (object_index != Players[Player_num].objnum || Players[Player_num].shields < 0 || Player_is_dead)) return 1;
	if (action->num_links < 0 || action->num_links > MAX_WALLS_PER_LINK) return 1;
	for (int i = 0; i < action->num_links; ++i)
		if (action->seg[i] < 0 || action->seg[i] > Highest_segment_index || action->side[i] < 0 || action->side[i] >= 6) return 1;
	const auto *previous = state_for(object_index);
	int speed = action->value < 0 ? last_speed : action->value;
	if (speed <= 0 || speed > 10) speed = 10;
	last_speed = speed;
	if (!action->value || !action->num_links) {
		// A release keeps the authored turn in progress, just as the source does
		const auto saved = previous ? *previous : xl_speed_boost_state{};
		xl_speed_boost_stop(object_index);
		if (saved.steps) {
			states[object_index] = saved;
			states[object_index].boosted = 0;
			states[object_index].speed = speed;
		}
	} else {
		const int destination = action->seg[0], side = action->side[0];
		auto state = previous ? *previous : xl_speed_boost_state{};
		state.owner = object_index;
		state.signature = actor.signature;
		state.boosted = 1;
		state.speed = speed;
		const auto target = xl_compat::compile_classic_lighting_side(destination, side);
		std::copy(target.center.begin(), target.center.end(), state.destination);
		store(state.source, actor.pos);
		auto direction = side_direction(destination, side);
		int velocity = 60 + 40 * speed;
		for (int wall = 0; index >= 0 && wall < Num_walls; ++wall) {
			if (wall_trigger_index(Walls[wall].trigger) != index) continue;
			const int from = Walls[wall].segnum, from_side = Walls[wall].sidenum;
			if (from < 0 || from > Highest_segment_index || from_side < 0 || from_side >= 6) return 1;
			const auto start = xl_compat::compile_classic_lighting_side(from, from_side);
			std::copy(start.center.begin(), start.center.end(), state.source);
			vms_vector difference{};
			const auto destination_point = vector(state.destination), source_point = vector(state.source);
			if (Segments[from].children[from_side] != destination || Segments[destination].children[side] != from)
				vm_vec_sub(&difference, &destination_point, &source_point);
			if (difference.x || difference.y || difference.z) direction = normalize(difference);
			else {
				Controls.vertical_thrust_time = Controls.forward_thrust_time = Controls.sideways_thrust_time = 0;
				velocity = 60;
				state.boosted = -1;
			}
			break;
		}
		set_velocity(actor, state, direction, velocity);
		if (action->flags & 32) {
			store(state.direction, side_direction(destination, side));
			orient(actor, state, 10);
			state.steps = 9;
			state.phase = 0;
		}
		states[object_index] = state;
	}
	if (actor.type == OBJ_PLAYER && shot && !(action->flags & TF_NO_MESSAGE)) HUD_init_message_literal(HM_DEFAULT, "Speed Boost!");
	return 0;
}

extern "C" void xl_speed_boost_frame(void)
{
	if (!xl_level_extensions_active()) return;
	for (int index = 0; index <= Highest_object_index; ++index) {
		auto *state = state_for(index);
		if (!state) continue;
		if (index == Players[Player_num].objnum && (Player_is_dead || Players[Player_num].shields < 0)) {
			xl_speed_boost_forget(index);
			continue;
		}
		if (!state->steps || FrameTime <= 0) continue;
		const auto phase = std::int64_t(state->phase) + std::int64_t(FrameTime) * 40;
		state->phase = static_cast<int>(phase % F1_0);
		// At most one source 40-Hz orientation tick per rendered simulation frame
		if (phase >= F1_0) orient(Objects[index], *state, state->steps--);
		if (!state->steps) state->phase = 0;
	}
}

extern "C" int xl_speed_boost_begin(int index)
{
	auto *state = state_for(index);
	if (!state) return 0;
	const auto velocity = Objects[index].mtype.phys_info.velocity;
	if (index == Players[Player_num].objnum && !velocity.x && !velocity.y && !velocity.z) state->boosted = 0;
	return state->boosted;
}

extern "C" int xl_speed_boost_retarget(int index, int32_t x, int32_t y, int32_t z)
{
	auto *state = state_for(index);
	if (!state || !state->boosted) return 0;
	const auto destination = vector(state->destination);
	const vms_vector start{ x, y, z };
	vms_vector direction;
	vm_vec_sub(&direction, &destination, &start);
	state->speed = last_speed ? last_speed : 10;
	last_speed = state->speed;
	set_velocity(Objects[index], *state, normalize(direction), 60 + 40 * state->speed);
	return 1;
}

extern "C" int xl_speed_boost_drag(int index, int32_t time)
{
	auto *state = state_for(index);
	if (!state || !state->boosted || time < 0) return 0;
	auto &physics = Objects[index].mtype.phys_info;
	if (!physics.velocity.x && !physics.velocity.y && !physics.velocity.z && index == Players[Player_num].objnum) {
		state->boosted = 0;
		return 0;
	}
	if (!(physics.flags & PF_USES_THRUST)) return 0;
	const auto acceleration = scale(physics.thrust, divide(F1_0, physics.mass));
	if (!acceleration.x && !acceleration.y && !acceleration.z) physics.velocity = vector(state->velocity);
	else {
		int count = time / step_time;
		const fix part = divide(time % step_time, step_time);
		while (count--) {
			vm_vec_add2(&physics.velocity, &acceleration);
			physics.velocity = scale(physics.velocity, F1_0 - physics.drag);
		}
		const auto extra = scale(acceleration, part);
		vm_vec_add2(&physics.velocity, &extra);
		physics.velocity = scale(physics.velocity, F1_0 - multiply(part, physics.drag));
		physics.velocity.x = std::clamp(physics.velocity.x, state->minimum[0], state->maximum[0]);
		physics.velocity.y = std::clamp(physics.velocity.y, state->minimum[1], state->maximum[1]);
		physics.velocity.z = std::clamp(physics.velocity.z, state->minimum[2], state->maximum[2]);
	}
	return 1;
}

extern "C" int xl_speed_boost_exit_blocked(int index, int segment, int side)
{
	if (index != Players[Player_num].objnum || xl_speed_boost_active(index) <= 0 || segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6) return 0;
	const int child = Segments[segment].children[side];
	if (child < 0 || !xl_speed_boost_segment(segment) || xl_speed_boost_segment(child)) return 0;
	const int wall = Segments[segment].sides[side].wall_num;
	if (wall < 0 || wall >= Num_walls) return 1;
	const int action = wall_trigger_index(Walls[wall].trigger);
	return action < 0 || action >= Num_triggers || Triggers[action].type != 15;
}

extern "C" int xl_speed_boost_last_speed(void)
{
	return xl_level_extensions_active() ? last_speed : 0;
}
extern "C" int xl_speed_boost_state_count(void)
{
	int count = 0;
	for (int index = 0; index <= Highest_object_index; ++index) count += state_for(index) != nullptr;
	return count;
}
extern "C" int xl_speed_boost_state_get(int row, xl_speed_boost_state *result)
{
	if (!result || row < 0) return 0;
	for (int index = 0; index <= Highest_object_index; ++index)
		if (const auto *state = state_for(index))
			if (!row--) {
				*result = *state;
				return 1;
			}
	return 0;
}
extern "C" int xl_speed_boost_restore(const xl_speed_boost_state *input, int count, int saved_speed, int apply)
{
	if (saved_speed < 0 || saved_speed > 10 || !xl_level_extensions_active() || count < 0 || count > MAX_OBJECTS || (!input && count)) return 0;
	int previous = -1;
	for (int row = 0; row < count; ++row) {
		const auto &state = input[row];
		if (state.owner <= previous || !live(state.owner) || Objects[state.owner].signature != state.signature || state.boosted < -1 || state.boosted > 1 || state.speed < 1 || state.speed > 10 || state.steps < 0 || state.steps > 9 || state.phase < 0 || state.phase >= F1_0 || (!state.steps && state.phase) || (!state.steps && !state.boosted)) return 0;
		if (!xl_speed_boost_destination_valid(state.destination)) return 0;
		previous = state.owner;
		for (int axis = 0; axis < 3; ++axis) {
			if (state.velocity[axis] < -460 * F1_0 || state.velocity[axis] > 460 * F1_0 || state.minimum[axis] != state.velocity[axis] - 60 * F1_0 || state.maximum[axis] != state.velocity[axis] + 60 * F1_0 || state.direction[axis] < -F1_0 || state.direction[axis] > F1_0) return 0;
		}
	}
	if (apply) {
		last_speed = saved_speed;
		states.fill({});
		for (int row = 0; row < count; ++row) states[input[row].owner] = input[row];
	}
	return 1;
}
