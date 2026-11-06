#ifndef XL_REFERENCE_SLOW_MOTION_PHYSICS_H
#define XL_REFERENCE_SLOW_MOTION_PHYSICS_H
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
static int slow_motion_physics(const char *path)
{
	std::ifstream input(path);
	std::string line;
	if (!std::getline(input, line) || line != "slow-motion-physics-v1") throw std::runtime_error("Missing slow-motion transcript header");
	std::set<std::tuple<int, int, int, int, int, fix, fix, fix, fix>> moves;
	std::set<std::tuple<int, int, fix, fix>> lives;
	while (std::getline(input, line)) {
		std::istringstream row(line);
		char kind;
		int profile, type;
		if (!(row >> kind >> profile) || profile < 0 || profile > 3) throw std::runtime_error("Invalid slow-motion profile");
		const float expected_world = profile == 0 ? 1 : profile == 1 ? 1.025f
		                                                             : 3;
		gameStates.gameplay.slowmo[0].fSpeed = expected_world;
		if (kind == 'L') {
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
	if (moves.size() != 768 || lives.size() != 144) throw std::runtime_error("Incomplete slow-motion source coverage");
	std::cout << "768 native offsets/lifetimes and 144 lifetime boundary cases match pinned source\n";
	return 0;
}
#endif
