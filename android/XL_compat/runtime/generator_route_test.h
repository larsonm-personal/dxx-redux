// Read-only observations of generators exercised by ordinary route inputs
extern "C" {
#include "fuelcen.h"
}

static std::vector<int> generator_checkpoint_state()
{
	std::vector<int> state{ Num_robot_centers, Num_fuelcenters };
	for (int i = 0; i < Num_robot_centers; ++i) {
		const auto &center = RobotCenters[i];
		const int station = matcen_get_station_num(i);
		require(station >= 0 && station < Num_fuelcenters, "generator has no valid native station");
		const auto &s = Station[station];
		state.insert(state.end(), { center.robot_flags[0], center.robot_flags[1], center.hit_points, center.interval,
		                            center.segnum, center.fuelcen_num, station, s.Type, s.segnum, s.Flag, s.Enabled,
		                            s.Lives, s.Capacity, s.MaxCapacity, s.Timer, s.Disable_time,
		                            s.Center.x, s.Center.y, s.Center.z });
	}
	return state;
}

struct GeneratorRouteObservation {
	std::set<std::pair<int, int>> seen;
	std::vector<std::pair<int, int>> states;
	unsigned spawns = 0, activations = 0;
	bool checkpoint_checked = false;

	GeneratorRouteObservation()
	{
		for (int i = 0; i < Num_robot_centers; ++i) {
			const int station = matcen_get_station_num(i);
			require(station >= 0 && station < Num_fuelcenters, "generator observation station missing");
			states.emplace_back(Station[station].Enabled, Station[station].Lives);
		}
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT) seen.emplace(Objects[i].matcen_creator, Objects[i].signature);
	}

	void observe()
	{
		require(states.size() == static_cast<std::size_t>(Num_robot_centers), "generator set changed during route");
		for (int i = 0; i < Num_robot_centers; ++i) {
			const auto &s = Station[matcen_get_station_num(i)];
			const auto state = std::make_pair(int(s.Enabled), int(s.Lives));
			if (state != states[i]) {
				if (s.Enabled && !states[i].first) ++activations;
				std::cout << "Route generator: center=" << i << " segment=" << s.segnum << " enabled=" << int(s.Enabled)
				          << " lives=" << int(s.Lives) << " capacity=" << s.Capacity << '\n';
				states[i] = state;
			}
		}
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &o = Objects[i];
			const auto creator = static_cast<unsigned char>(o.matcen_creator);
			if (o.type != OBJ_ROBOT || creator == 255 || !(creator & 0x80) || !seen.emplace(o.matcen_creator, o.signature).second) continue;
			const int station = creator ^ 0x80;
			int center = -1;
			for (int n = 0; n < Num_robot_centers; ++n)
				if (matcen_get_station_num(n) == station) center = n;
			require(center >= 0 && o.id < 64, "spawned robot has unknown generator provenance");
			require((static_cast<unsigned>(RobotCenters[center].robot_flags[o.id / 32]) & (1u << (o.id % 32))) != 0,
			        "generator spawned a robot outside its authored mask");
			++spawns;
			std::cout << "Route generator spawn: center=" << center << " robot=" << int(o.id) << " signature=" << o.signature << '\n';
		}
		if (spawns && !checkpoint_checked) {
			const auto before = generator_checkpoint_state();
			const auto robots = [] {
				std::vector<int> values;
				for (int i = 0; i <= Highest_object_index; ++i) {
					const auto &o = Objects[i];
					if (o.type == OBJ_ROBOT)
						values.insert(values.end(), { i, o.signature, o.id, o.matcen_creator, o.shields, o.segnum, o.pos.x, o.pos.y, o.pos.z });
				}
				return values;
			};
			const auto before_robots = robots();
			const auto energy = Players[Player_num].energy, shields = Players[Player_num].shields;
			char save[] = "xl-active-generator.sav", description[] = "Earned active generator";
			stop_time();
			require(state_save_all_sub(save, description) && state_restore_all_sub(save, 0), "active generator checkpoint failed");
			require(generator_checkpoint_state() == before && robots() == before_robots &&
			            Players[Player_num].energy == energy && Players[Player_num].shields == shields,
			        "active generator restore changed timers, robot provenance or earned resources");
			checkpoint_checked = true;
			std::cout << "Active route generator checkpoint: timers, capacity, lives, masks, spawned robot provenance and earned resources restored\n";
		}
	}
};
