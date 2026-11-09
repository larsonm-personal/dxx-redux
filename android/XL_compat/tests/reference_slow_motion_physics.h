#ifndef XL_REFERENCE_SLOW_MOTION_PHYSICS_H
#define XL_REFERENCE_SLOW_MOTION_PHYSICS_H
#include <algorithm>
#include <set>
#include <tuple>
constexpr fix IMMORTAL_TIME = 0x3fffffff, ONE_FRAME_TIME = 0x3ffffffe;
struct Lifetime {
	struct {
		fix xLifeLeft;
	} info;
	void update()
	{
#include "xl_reference_slow_lifetime.h"
	}
};
#include "xl_reference_slow_omega_constants.h"
#include "xl_reference_slow_omega_energy.h"
static fix slow_omega_step()
{
#include "xl_reference_slow_omega_step.h"
}
static fix slow_homing_step(fix frameTime)
{
#include "xl_reference_slow_homing_step.h"
}
static int primary_deadline(fix last, fix next, fix now, float speed)
{
	float s;
#include "xl_reference_slow_primary_deadline.h"
}
static int flare_deadline(fix &next, fix now, float speed, bool enough)
{
#include "xl_reference_slow_flare_deadline.h"
}
struct tAILocalInfo {
	fix pNextrimaryFire, nextSecondaryFire, timeSinceProcessed;
};
struct tRobotInfo {
	int nSecWeaponType;
};
struct tAIStateInfo {
	tAILocalInfo *pLocalInfo;
	tRobotInfo *pRobotInfo;
};
#include "xl_reference_slow_ai.h"
#include "xl_reference_slow_saturate.h"
#include "reference_contact_control_flow.h"
struct Rotation : CObject {
	void turn(CAngleVector dest_angles, CAngleVector cur_angles, fix rate)
	{
		fix delta_p, delta_h;
		auto &pvRotVel = mType.physInfo.rotVel;
#include "xl_reference_slow_turn.h"
	}
	CAngleVector scale(CAngleVector turnAngles)
	{
#include "xl_reference_slow_rotation.h"
		return turnAngles;
	}
};
static int slow_motion_physics(const char *path)
{
	std::ifstream input(path);
	std::string line;
	if (!std::getline(input, line) || line != "slow-motion-physics-v1") throw std::runtime_error("Missing slow-motion transcript header");
	std::set<std::tuple<int, int, int, int, int, fix, fix, fix, fix>> moves;
	std::set<std::tuple<int, int, fix, fix>> lives;
	std::set<std::tuple<int, int, int, int>> rotations;
	std::set<std::tuple<int, int, int, int, int, int, int>> turns;
	std::set<std::tuple<int, int, int, int, int>> contacts;
	std::set<std::tuple<int, int, int, int, int, int>> ai;
	std::set<std::tuple<int, int, int, int, int, int>> ai_turns;
	std::set<std::tuple<int, int, int, int, int, int>> omega;
	std::set<std::tuple<int, int, int, int, int>> homing;
	std::set<std::tuple<int, int, int, int, int, int>> deadlines;
	while (std::getline(input, line)) {
		std::istringstream row(line);
		char kind;
		int profile, type;
		if (!(row >> kind >> profile) || profile < 0 || profile > 3) throw std::runtime_error("Invalid slow-motion profile");
		const float expected_world = profile == 0 ? 1 : profile == 1 ? 1.025f
		                                                             : 3;
		gameStates.gameplay.slowmo[0].fSpeed = expected_world;
		if (kind == 'D') {
			int multiplayer, li, ni, epoch, enough, primary, flare, next_actual;
			if (!(row >> multiplayer >> li >> ni >> epoch >> enough >> primary >> flare >> next_actual) ||
			    (multiplayer != 0 && multiplayer != 1) || li < 0 || li >= 4 || ni < 0 || ni >= 8 ||
			    (epoch != 0 && epoch != 1) || (enough != 0 && enough != 1) ||
			    !deadlines.emplace(profile, multiplayer, li, ni, epoch, enough).second) throw std::runtime_error("Invalid/duplicate firing deadline control");
			const fix lasts[] = { -I2X(2), -1, 0, I2X(1) };
			const fix nexts[] = { -1, 0, 1, I2X(1) / 4, I2X(2) - 1, I2X(2), I2X(2) + 1, I2X(6) };
			fix next = nexts[ni];
			const auto expected_primary = multiplayer ? int(next <= 0) : primary_deadline(lasts[li], next, 0, expected_world);
			int expected_flare;
			if (multiplayer) {
				expected_flare = next <= 0;
				if (expected_flare) next = enough ? I2X(1) / 4 : I2X(2);
			} else expected_flare = flare_deadline(next, 0, expected_world, enough);
			if (primary != expected_primary || flare != expected_flare || next_actual != next) throw std::runtime_error("Native firing deadline differs from pinned source");
		} else if (kind == 'O') {
			int multiplayer, difficulty, time, charge, energy, actual_charge, actual_energy;
			if (!(row >> multiplayer >> difficulty >> time >> charge >> energy >> actual_charge >> actual_energy) ||
			    (multiplayer != 0 && multiplayer != 1) || difficulty < 0 || difficulty > 4 ||
			    (time != 1 && time != 3 && time != 4 && time != 7 && time != 15 && time != 65 && time != 1092 && time != 16384) ||
			    (charge != 0 && charge != 1 && charge != I2X(1) - 1 && charge != I2X(1)) || (energy != 0 && energy != 1 && energy != I2X(100)) ||
			    !omega.emplace(profile, multiplayer, difficulty, time, charge, energy).second) throw std::runtime_error("Invalid/duplicate Omega clock control");
			gameStates.gameplay.slowmo[0].fSpeed = multiplayer ? 1 : expected_world;
			gameStates.app.nDifficultyLevel = difficulty;
			gameData.timeData.xFrame = time;
			const fix result = energy ? std::min(I2X(1), charge + slow_omega_step()) : charge;
			const fix debit = OmegaEnergy(result - charge);
			if (actual_charge != result || actual_energy != std::max(0, energy - debit)) throw std::runtime_error("Native Omega increment/debit differs from pinned source");
		} else if (kind == 'H') {
			int multiplayer, console, goal, time, actual;
			if (!(row >> multiplayer >> console >> goal >> time >> actual) || (multiplayer != 0 && multiplayer != 1) || (console != 0 && console != 1) || goal < 0 || goal >= 4 ||
			    (time != 1 && time != 3 && time != 4 && time != 7 && time != 15 && time != 65 && time != 1092 && time != 16384) ||
			    !homing.emplace(profile, multiplayer, console, goal, time).second) throw std::runtime_error("Invalid/duplicate homing clock control");
			gameStates.gameplay.slowmo[0].fSpeed = multiplayer ? 1 : expected_world;
			if (actual != slow_homing_step(time)) throw std::runtime_error("Native homing orientation scale differs from pinned source");
		} else if (kind == 'B') {
			int multiplayer, console, goal, rate, time, actual_dot, actual_scale;
			if (!(row >> multiplayer >> console >> goal >> rate >> time >> actual_dot >> actual_scale) ||
			    (multiplayer != 0 && multiplayer != 1) || (console != 0 && console != 1) || goal < 0 || goal >= 4 ||
			    (rate != I2X(1) / 4 && rate != I2X(1) && rate != I2X(4)) || (time != I2X(1) / 1000 && time != I2X(1) / 60 && time != I2X(1) / 4) ||
			    !ai_turns.emplace(profile, multiplayer, console, goal, rate, time).second) throw std::runtime_error("Invalid/duplicate AI turn control");
			IsMultiGame = multiplayer;
			fix dot = goal == 0 ? 0 : goal == 1 ? I2X(1)
			                      : goal == 2   ? -I2X(1)
			                                    : 65528;
			fix new_scale = FixDiv(time, rate);
#include "xl_reference_slow_ai_dot.h"
#include "xl_reference_slow_ai_new_scale.h"
			if (dot != actual_dot || new_scale != actual_scale) throw std::runtime_error("AI world-clock turn differs from pinned source");
		} else if (kind == 'A') {
			int multiplayer, secondary, sample, time, elapsed, primary_result, secondary_result, elapsed_result;
			if (!(row >> multiplayer >> secondary >> sample >> time >> elapsed >> primary_result >> secondary_result >> elapsed_result) ||
			    (multiplayer != 0 && multiplayer != 1) || (secondary != -1 && secondary != 0) || sample < 0 || sample >= 11 ||
			    (time != I2X(1) / 1000 && time != I2X(1) / 60 && time != I2X(1) / 4) ||
			    (elapsed != 0 && elapsed != I2X(256) - 1 && elapsed != I2X(256)) ||
			    !ai.emplace(profile, multiplayer, secondary, sample, time, elapsed).second) throw std::runtime_error("Invalid/duplicate AI clock control");
			const fix floor = fix(I2X(8) * expected_world);
			const fix times[] = { -I2X(24) - 1, -I2X(24), -I2X(24) + 1, -I2X(8) - 1, -I2X(8), -I2X(8) + 1, -floor - 1, -floor, -floor + 1, 0, I2X(1) };
			tAILocalInfo local{ times[sample], times[sample] - 1, elapsed };
			tRobotInfo robot{ secondary };
			tAIStateInfo state{ &local, &robot };
			gameStates.gameplay.slowmo[0].fSpeed = multiplayer ? 1 : expected_world;
			gameData.timeData.xFrame = time;
			AINextFireHandler(&object, &state);
			if (local.pNextrimaryFire != primary_result || local.nextSecondaryFire != secondary_result || local.timeSinceProcessed != elapsed_result)
				throw std::runtime_error("Native AI frame timers differ from pinned source at control " + std::to_string(ai.size()));
		} else if (kind == 'C') {
			int console, multiplayer, room, bounce;
			if (!(row >> console >> multiplayer >> room >> bounce) || (console != 0 && console != 1) ||
			    (multiplayer != 0 && multiplayer != 1) || (room != 60 && room != 61) || (bounce != 0 && bounce != 1) ||
			    !contacts.emplace(profile, console, multiplayer, room, bounce).second)
				throw std::runtime_error("Invalid/duplicate contact control");
			CFixVector values[6];
			for (auto &v : values)
				if (!(row >> v.v.coord.x >> v.v.coord.y >> v.v.coord.z)) throw std::runtime_error("Truncated contact vector");
			for (const auto &v : values)
				for (fix component : { v.v.coord.x, v.v.coord.y, v.v.coord.z })
					if (std::int64_t(component) < -(1LL << 27) || std::int64_t(component) > (1LL << 27))
						throw std::runtime_error("Contact coordinate outside qualified fixture domain");
			const auto normal = values[3].v.coord;
			if (std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z) != I2X(1) ||
			    (normal.x && normal.x != I2X(1) && normal.x != -I2X(1)) ||
			    (normal.y && normal.y != I2X(1) && normal.y != -I2X(1)) ||
			    (normal.z && normal.z != I2X(1) && normal.z != -I2X(1)) ||
			    values[1].v.coord.x != -4 * normal.x || values[1].v.coord.y != -4 * normal.y || values[1].v.coord.z != -4 * normal.z)
				throw std::runtime_error("Contact is not a qualified axial impact");
			Contact actor;
			gameData.objData.pConsole = console ? &actor : &objects[1];
			gameStates.gameplay.slowmo[1].fSpeed = profile == 3 ? 1 : expected_world;
			IsMultiGame = multiplayer;
			gameData.physicsData.xTime = I2X(1) / 4;
			actor.Velocity() = values[1];
			CPhysSimData state;
			state.xSimTime = state.xOldSimTime = gameData.physicsData.xTime;
			state.xTimeScale = !multiplayer && room == 60 ? 75 : 100;
			state.vStartPos = state.vOldPos = values[0];
			actor.info.position.vPos = values[2];
			actor.UpdateOffset(state);
			actor.ComputeMovedTime(state);
			if (state.xSimTime <= 0 || state.xSimTime >= state.xOldSimTime) throw std::runtime_error("Contact did not leave a retry interval");
			if (bounce) {
				const fix wall_part = CFixVector::Dot(values[3], actor.Velocity());
				actor.Velocity() -= values[3] * (2 * wall_part);
				actor.UpdateOffset(state);
				actor.info.position.vPos += state.vOffset;
			} else {
				// Native controls require death after this ordinary laser wall hit
				// Execute the pinned early return before considering FixPosition
				actor.info.nFlags = OF_SHOULD_BE_DEAD;
				state.hitResult.nType = HIT_WALL;
				actor.finish_wall_contact(state);
			}
			const auto same = [](const CFixVector &a, const CFixVector &b) {
				return a.v.coord.x == b.v.coord.x && a.v.coord.y == b.v.coord.y && a.v.coord.z == b.v.coord.z;
			};
			if (!same(actor.info.position.vPos, values[4]) || !same(actor.Velocity(), values[5]))
				throw std::runtime_error("Native contact/retry differs from pinned source at control " + std::to_string(contacts.size()));
		} else if (kind == 'T') {
			int console, multiplayer, morph, goal, rate, sign, dp, dh, cp, ch, x, y, z;
			if (!(row >> console >> multiplayer >> morph >> goal >> rate >> sign >> dp >> dh >> cp >> ch >> x >> y >> z) ||
			    (console != 0 && console != 1) || (multiplayer != 0 && multiplayer != 1) || (morph != 0 && morph != 1) ||
			    goal < 0 || goal >= 8 || (rate != I2X(1) / 4 && rate != I2X(1) && rate != I2X(4)) || (sign != -1 && sign != 1) ||
			    dp < -32768 || dp > 32767 || dh < -32768 || dh > 32767 || cp < -32768 || cp > 32767 || ch < -32768 || ch > 32767 ||
			    !turns.emplace(profile, console, multiplayer, morph, goal, rate, sign).second)
				throw std::runtime_error("Invalid/duplicate turn-impulse control");
			Rotation actor;
			gameData.objData.pConsole = console ? &actor : &objects[1];
			gameStates.gameplay.slowmo[1].fSpeed = profile == 3 ? 1 : expected_world;
			IsMultiGame = multiplayer;
			CAngleVector destination{}, current{};
			destination.v.coord.p = fixang(dp);
			destination.v.coord.h = fixang(dh);
			current.v.coord.p = fixang(cp);
			current.v.coord.h = fixang(ch);
			actor.mType.physInfo.rotVel.Set(sign * 8191, -sign * 8192, 123);
			actor.turn(destination, current, rate * (morph ? 2 : 1));
			const auto result = actor.mType.physInfo.rotVel.v.coord;
			if (x != result.x || y != result.y || z != result.z) throw std::runtime_error("Native turn impulse differs from pinned source");
		} else if (kind == 'R') {
			int console, multiplayer, angle, p, h, b;
			if (!(row >> console >> multiplayer >> angle >> p >> h >> b) ||
			    (console != 0 && console != 1) || (multiplayer != 0 && multiplayer != 1) ||
			    (angle != -32768 && angle != -32767 && angle != -1001 && angle != -1 && angle != 0 && angle != 1 && angle != 1001 && angle != 32766 && angle != 32767) ||
			    !rotations.emplace(profile, console, multiplayer, angle).second)
				throw std::runtime_error("Invalid/duplicate rotation control");
			Rotation actor;
			gameData.objData.pConsole = console ? &actor : &objects[1];
			gameStates.gameplay.slowmo[1].fSpeed = profile == 3 ? 1 : expected_world;
			IsMultiGame = multiplayer;
			CAngleVector angles{};
			angles.v.coord.p = fixang(angle);
			angles.v.coord.h = fixang(-angle - 1);
			angles.v.coord.b = fixang(angle / 3);
			angles = actor.scale(angles);
			if (p != angles.v.coord.p || h != angles.v.coord.h || b != angles.v.coord.b)
				throw std::runtime_error("Native rotation differs from pinned source");
		} else if (kind == 'L') {
			float world;
			fix time, before, actual;
			if (!(row >> type >> world >> time >> before >> actual) || world != expected_world ||
			    (type != 2 && type != 4 && type != 5 && type != 7) ||
			    (time != 0 && time != -1 && time != I2X(1) / 60) ||
			    (before != IMMORTAL_TIME && before != ONE_FRAME_TIME && before != I2X(100)) ||
			    !lives.emplace(profile, type, time, before).second) throw std::runtime_error("Invalid/duplicate lifetime control");
			gameData.physicsData.xTime = time;
			Lifetime life{ { before } };
			life.update();
			if (life.info.xLifeLeft != actual) throw std::runtime_error("Native lifetime sentinel differs from pinned source");
		} else if (kind == 'M') {
			int segment_id, console, multiplayer, wet;
			float world, player;
			fix time, values[6], remaining;
			if (!(row >> segment_id >> world >> player >> console >> multiplayer >> type >> wet >> time)) throw std::runtime_error("Truncated slow-motion row");
			for (auto &value : values)
				if (!(row >> value)) throw std::runtime_error("Truncated slow-motion vector");
			if (!(row >> remaining) || world != expected_world || player != (profile == 3 ? 1 : expected_world) ||
			    (console != 0 && console != 1) || (multiplayer != 0 && multiplayer != 1) ||
			    (segment_id != 60 && segment_id != 61) || wet != (!multiplayer && segment_id == 60) ||
			    (type != 2 && type != 4 && type != 5 && type != 7) ||
			    (time != I2X(1) / 1000 && time != I2X(1) / 60 && time != I2X(1) / 4) ||
			    !((values[0] == I2X(1) && values[1] == 0 && values[2] == 0) ||
			      (values[0] == -1001 && values[1] == 2003 && values[2] == -3007)) ||
			    !moves.emplace(profile, segment_id, console, multiplayer, type, time, values[0], values[1], values[2]).second)
				throw std::runtime_error("Invalid/duplicate slow-motion case");
			IsMultiGame = multiplayer;
			// Multiplayer level startup resets both reference clocks; stale singleplayer
			// state is deliberately bypassed by Redux's network isolation guard
			gameStates.gameplay.slowmo[0].fSpeed = multiplayer ? 1 : world;
			gameStates.gameplay.slowmo[1].fSpeed = multiplayer ? 1 : player;
			gameData.objData.pConsole = console ? &object : &objects[1];
			object.info.nType = type;
			object.info.nSegment = 0;
			object.mType.physInfo.flags = 0;
			object.mType.physInfo.mass = I2X(1);
			object.mType.physInfo.drag = 0;
			object.mType.physInfo.thrust.Set(0, 0, 0);
			object.mType.physInfo.velocity.Set(values[0], values[1], values[2]);
			segment.water = wet;
			segment.lava = false;
			gameData.physicsData.xTime = time;
			CPhysSimData state;
			state.Setup();
			object.UpdateOffset(state);
			const fix expected[] = { state.vOffset.v.coord.x, state.vOffset.v.coord.y, state.vOffset.v.coord.z };
			for (unsigned axis = 0; axis < 3; ++axis)
				if (values[axis + 3] != expected[axis]) throw std::runtime_error("Native slow displacement differs from pinned source at row " + std::to_string(moves.size()));
			Lifetime life{ { I2X(100) } };
			life.update();
			if (life.info.xLifeLeft != remaining) throw std::runtime_error("Native world-clock lifetime differs");
		} else throw std::runtime_error("Unknown slow-motion record");
		std::string extra;
		if (row >> extra) throw std::runtime_error("Trailing slow-motion row data");
	}
	if (moves.size() != 768 || lives.size() != 144 || rotations.size() != 144 || turns.size() != 1536 || contacts.size() != 64 || ai.size() != 1584 || ai_turns.size() != 576 || omega.size() != 3840 || homing.size() != 512 || deadlines.size() != 1024) throw std::runtime_error("Incomplete slow-motion source coverage");
	std::cout << "768 native offsets/lifetimes and 144 lifetime boundary cases and 144 rotations and 1536 turn impulses and 64 contacts and 1584 AI frames and 576 AI turns, 3840 Omega steps and 512 homing orientations and 1024 firing deadline cases match pinned source\n";
	return 0;
}
#endif
