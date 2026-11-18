#include "recorded_future_test.h"

static void equipment_mask_lifecycle(const std::filesystem::path &root, int id)
{
	const std::set<int> admitted{ 7, 8, 9, 24, 26, 27, 46, 47 };
	require(admitted.count(id) != 0, "Unknown inert equipment lifecycle item");
	const auto authored = write_equipment_fixture(root, 23, id);
	const std::string mission = "eq" + std::to_string(id);
	char error[256]{};
	require(load_base_mission(mission.c_str(), error, sizeof(error)), error);
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	Difficulty_level = 2;
	StartNewGame(1);
	Game_suspended = 0;
	require(xl_level_extensions_active() && xl_equipment_count() == 2, "Inert equipment source did not load");
	const auto state = [] {
		xl_equipment_state value;
		require(xl_equipment_state_get(0, &value), "Inert equipment state missing");
		return value;
	};
	const auto item = [&] {
		int found = -1;
		for (int i = Segments[20].objects; i >= 0; i = Objects[i].next)
			if (Objects[i].type == OBJ_POWERUP) {
				require(found < 0 && Objects[i].id == id, "Inert equipment selected an unauthored or duplicate item");
				found = i;
			}
		return found;
	};
	const auto verify_item = [&] {
		const int index = item();
		require(index >= 0, "Authored inert equipment did not produce its item");
		const auto &object = Objects[index];
		require(object.control_type == CT_POWERUP && object.movement_type == MT_PHYSICS && object.render_type == RT_POWERUP &&
		            object.size == Powerup_info[id].size && object.lifeleft == IMMORTAL_TIME &&
		            object.rtype.vclip_info.vclip_num == Powerup_info[id].vclip_num && object.matcen_creator == authored.segments[20].owner,
		        "Authored inert equipment lost its native resource or creation attributes");
		return index;
	};
	const auto cold_restore = [&](const char *label) {
		const auto maker = xl_compat::equipment_snapshot();
		const auto objects = [&] {
			nlohmann::json value = nlohmann::json::array();
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_POWERUP && Objects[i].segnum == 20)
					value.push_back(nlohmann::json(input_demo_object_trace_snapshot(Objects[i], i)));
			return value;
		};
		const auto before = objects();
		char save[] = "inert-item.sav", description[] = "Inert equipment";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save authored inert equipment");
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(!xl_equipment_count() && xl_compat::equipment_snapshot().at("makers").empty(), "Inert equipment owner leaked into stock");
		require(state_restore_all_sub(save, 0), "Cannot cold restore authored inert equipment");
		const auto after = objects();
		native_continuation_receipt(root.parent_path() / (mission + "-" + label + ".json"),
		                            { { "before", before }, { "after", after }, { "maker_before", maker }, { "maker_after", xl_compat::equipment_snapshot() } });
		require(maker == xl_compat::equipment_snapshot() && before == after, "Inert equipment cold restore changed items, mask or earned producer state");
	};
	// Controlled pose setup keeps the player away from the generator during production
	obj_relink(ConsoleObject - Objects, 0);
	compute_segment_center(&ConsoleObject->pos, &Segments[0]);
	require(!state().enabled && item() < 0 && xl_equipment_trigger(20) == 1, "Authored inert equipment activation differs");
	FrameTime = i2f(9) + 1;
	fuelcen_update_all();
	require(state().pending && state().timer == 0 && item() < 0, "Authored inert equipment did not earn its pending phase");
	cold_restore("pending");
	// Real capture begins from the earned pending state and uses only zero input
	xl_recorded_future(root.parent_path(), mission.c_str());
	const int produced = verify_item();
	require(!state().pending, "Inert equipment retained pending state after creation");
	const auto player_before = Players[Player_num];
	for (int contact = 0; contact < 2; ++contact)
		collide_player_and_powerup(ConsoleObject, &Objects[produced], &Objects[produced].pos);
	require(!(Objects[produced].flags & OF_SHOULD_BE_DEAD) && !std::memcmp(&player_before, &Players[Player_num], sizeof(player_before)),
	        "Authored inert equipment contact rewarded or consumed an item");
	cold_restore("spawned");
	verify_item();
	FrameTime = i2f(9);
	fuelcen_update_all();
	require(!state().pending && state().timer == 0, "Unconsumed authored item did not block production");
	// Explicit test removal models a freed producer; no mask or RNG is rewritten
	obj_delete(item());
	FrameTime = i2f(9);
	fuelcen_update_all();
	require(state().pending, "Freed inert producer did not restart its morph phase");
	FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
	fuelcen_update_all();
	verify_item();
	std::cout << "Authored inert equipment " << id << ": incoming ownership, pending/spawned cold restore, recorded future, contact, occupancy and replenishment passed\n";
}
