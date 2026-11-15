#ifndef DXX_XL_MISSING_ACTION_TEST_H
#define DXX_XL_MISSING_ACTION_TEST_H
#include "../tests/named_action_receipt.h"
namespace xl_missing_action_control
{
inline void run(const std::filesystem::path &root, int ordinal)
{
	using namespace xl_compat;
	using namespace named_audio_control;
	char error[256]{};
	require(load_base_mission("xactmiss", error, sizeof(error)), error);
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(ordinal);
	SDL_PauseAudio(1);
	digi_stop_digi_sounds();
	GameTime64 = 10 * F1_0;
	const auto initial = snapshot();
	const auto unavailable = named_audio_diagnostics().at("unavailable_action_output");
	require(unavailable.size() == 4 && initial.at("events").size() == 11 && initial.at("voices").empty(), "Missing action fixture lost authored resources or created playback");
	std::array<trigger, 10> authored;
	std::copy(Triggers, Triggers + 10, authored.begin());
	const auto reset = [&] {
		SDL_PauseAudio(1);
		require(restore_named_audio(initial, true), "Cannot restore absent action owner");
		std::copy(authored.begin(), authored.end(), Triggers);
		xl_wall_trigger_reset();
	};
	uint32_t sim;
	require(d_rand_get_state(&sim), "Cannot observe absent action SIM state");
	const auto sim_calls = d_rand_get_call_count();
	std::vector<xl_named_action_receipt::Case> cases;
	for (int owner : { 0, 1, 2, 3, 4, 6, 7, 8, 9 }) {
		reset();
		for (const int delta : { 0, 0, 1, F1_0 }) {
			GameTime64 += delta;
			const auto before = event(owner);
			const auto failed = named_audio_diagnostics().at("failed_starts").get<unsigned>();
			const auto played = xl_named_audio_action(&Triggers[owner], owner);
			const auto after = event(owner);
			const auto operated = [&](const auto &row) { return row[1] == -1 ? -1 : int(GameTime64 - row[1].template get<std::int64_t>()); };
			const char *names[]{ "sound.wav", "long.wav", "mono.wav", "tiny.wav" };
			cases.push_back({ { Triggers[owner].time, !!(Triggers[owner].flags & TF_DISABLED), int(GameTime64), operated(before), before[2].get<int>(), before[5].get<bool>(), played,
			                    operated(after), after[2].get<int>(), after[5].get<bool>(), int(named_audio_diagnostics().at("failed_starts").get<unsigned>() - failed) },
			                  names[Triggers[owner].value / F1_0 - 1] });
			require(snapshot().at("voices").empty() && snapshot().at("next") == initial.at("next") && snapshot().at("cursor") == initial.at("cursor"), "Absent named action allocated a voice, serial or cursor");
		}
	}
	uint32_t after_sim;
	require(d_rand_get_state(&after_sim) && after_sim == sim && d_rand_get_call_count() == sim_calls, "Failed action output consumed SIM RNG");
	std::ofstream receipt(root / "missing-action.txt");
	xl_named_action_receipt::write(receipt, cases);
	require(bool(receipt), "Cannot preserve absent native action receipts");
	reset();
	check_trigger(&Segments[Walls[0].segnum], short(Walls[0].sidenum), short(Players[Player_num].objnum), 0);
	require(event(0)[1] == 0 && event(0)[2] == -1 && !event(0)[5].get<bool>(), "Wall failure fabricated duration or playing state");
	reset();
	require(check_trigger_sub(5, Player_num, 1) == 0 && event(0)[1] == 0, "Master omitted failed sound dispatch");
	reset();
	require(check_trigger_sub(3, Player_num, 1) == 0 && (Triggers[3].flags & TF_DISABLED) && !event(3)[5].get<bool>(), "Failed one-shot sound lost its disable flag");
	reset();
	check_trigger_sub(8, Player_num, 1);
	GameTime64 += 10 * F1_0;
	xl_named_audio_wall_frame(8);
	require(!(Triggers[8].flags & TF_DISABLED) && event(8)[2] == -1 && !event(8)[5].get<bool>(), "Failed permanent sound fabricated duration or expiry flags");
	reset();
	xl_wall_trigger_frame();
	require((Triggers[4].flags & TF_DISABLED) && event(4)[1] == 0 && event(255)[1] == 0 && !event(4)[5].get<bool>() && !event(255)[5].get<bool>(), "Failed wall/object autoplay lost dispatch or fabricated playing state");
	const auto autoplay = snapshot();
	xl_wall_trigger_frame();
	require(snapshot() == autoplay, "Failed autoplay repeated after its one-shot dispatch");
	int owner = -1;
	for (int index = 0; index < xl_object_trigger_count(); ++index) {
		xl_object_trigger_state state{};
		require(xl_object_trigger_state_get(index, &state), "Cannot inspect absent action object owner");
		if (state.value == F1_0) owner = state.owner;
	}
	require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Missing action removal owner absent");
	Objects[owner].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	require(Objects[owner].type == OBJ_NONE && event(254)[1] == 0 && !event(254)[5].get<bool>(), "Object removal omitted failed action dispatch");
	begin_capture();
	xl_named_audio_pause(0);
	run_output(2048);
	require(std::all_of(captured.begin(), captured.begin() + captured_samples, [](auto sample) { return sample == 0; }), "Unavailable action emitted fabricated PCM");
	const auto earned = snapshot();
	for (unsigned variant = 0; variant < 4; ++variant) {
		auto bad = earned;
		auto &row = bad["events"][0];
		if (variant == 0) row[2] = 0;
		if (variant == 1) {
			row[3] = 1;
			bad["next"] = 2;
		}
		if (variant == 2) row[4] = 44100;
		if (variant == 3) row[5] = nlohmann::json::boolean_t(1);
		const bool accepted = restore_named_audio(bad, true);
		const std::string reason = xl_named_audio_error();
		const auto after = snapshot();
		if (accepted || reason.find("fabricated playback or duration") == std::string::npos || after != earned)
			throw std::runtime_error("Absent action saved-state variant " + std::to_string(variant) + " accepted=" + std::to_string(accepted) + " reason=" + reason + " before=" + earned.dump() + " after=" + after.dump());
	}
	const auto malformed = root / "mods/xactmiss/sounds/sound.wav";
	require(!std::filesystem::exists(malformed), "Absent action control refuses to replace an existing file");
	std::filesystem::create_directories(malformed.parent_path());
	write_bytes(malformed.string().c_str(), { 0, 1, 2 });
	const bool rejected = !xl_level_supported(Gamesave_current_filename, 0) && std::strstr(xl_level_rejection(), "Invalid XL named sound");
	std::filesystem::remove(malformed);
	require(rejected && snapshot() == earned, "Malformed action sound was treated as absent or changed live ownership");
	std::array<unsigned, 10> flags;
	for (unsigned i = 0; i < flags.size(); ++i) flags[i] = Triggers[i].flags;
	char save[] = "absent-action.sav", description[] = "Absent action output";
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save failed action dispatch state");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Absent action stock transition failed");
	require(state_restore_all_sub(save, 0) && snapshot() == earned, "Disk-through-stock restore changed failed action timing or fabricated playback");
	for (unsigned i = 0; i < flags.size(); ++i) require(Triggers[i].flags == flags[i], "Absent action restore changed trigger flags");
	require(!game_is_time_paused(), "Absent action restore left an unbalanced game pause");
	std::cout << "Missing named actions passed: wall/master/object/autoplay, source attempt receipts, no fabricated PCM/duration, flags, malformed rejection and disk-through-stock\n";
}
} // namespace xl_missing_action_control
#endif
