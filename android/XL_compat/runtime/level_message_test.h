#include "level_messages.h"

static xl_message_state message_snapshot()
{
	xl_message_state state{};
	xl_message_state_get(&state);
	return state;
}
static void message_expect(int id, int seconds, const char *text)
{
	const auto state = message_snapshot();
	require(state.id == id && state.duration == seconds && !std::strcmp(xl_message_text(), text), "Message event lost ID, duration or exact text");
}
static void message_bad_state(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("message-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture owned message trailer");
	timer_delay(F1_0 / 50);
	file = PHYSFS_openWrite("message-same-frame.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture the same message frame twice");
	const auto user = root / "preview-user";
	std::ifstream input(user / "message-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	std::ifstream second(user / "message-same-frame.bin", std::ios::binary);
	const xl_compat::Bytes same_frame{ std::istreambuf_iterator<char>(second), std::istreambuf_iterator<char>() };
	require(encoded == same_frame, "Message serializer sampled wall time instead of capturing the frame's owned clock");
	const auto before = message_snapshot();
	const auto lights = light_snapshot();
	for (int variant = 0; variant < 14; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &row = block["message"];
		if (variant == 0) row[0] = "changed";
		if (variant == 1) row[1] = 99;
		if (variant == 2) row[2] = 99;
		if (variant == 3) row[3] = -1;
		if (variant == 4) row[3] = INT64_MAX;
		if (variant == 5) row[3] = UINT64_MAX;
		if (variant == 6) row[3] = true;
		if (variant == 7) row[3] = 1.5;
		if (variant == 8) row[1] = 0;
		if (variant == 9) row.push_back(0);
		if (variant == 10) row[1] = INT64_MAX;
		if (variant == 11) block.erase("message");
		if (variant == 12) row = nullptr;
		if (variant == 13) row[3] = std::int64_t(before.duration) * 1000;
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t sum = 2166136261u;
		for (unsigned char byte : text) sum = (sum ^ byte) * 16777619u;
		for (unsigned b = 0; b < 4; ++b) {
			bad[8 + b] = static_cast<unsigned char>(text.size() >> (8 * b));
			bad[12 + b] = static_cast<unsigned char>(sum >> (8 * b));
		}
		write_bytes((user / "message-bad.bin").string().c_str(), bad);
		file = PHYSFS_openRead("message-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "Malformed message state was accepted");
		const auto after = message_snapshot();
		require(after.id == before.id && after.duration == before.duration && after.elapsed_ms == before.elapsed_ms && same_light_snapshot(lights, light_snapshot()), "Message rejection partially applied runtime state");
	}
	file = PHYSFS_openRead("message-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file), "Valid message trailer did not recover");
	std::cout << "Fourteen malformed message trailers rejected atomically; valid trailer recovered\n";
}

static int level_message_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		for (int ordinal : { 1, 2 }) {
			require(load_base_mission("xmessage", error, sizeof(error)), error);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(ordinal);
			Game_suspended = SUSP_ROBOTS;
			require(xl_source_level_version() == (ordinal == 1 ? 23 : 27) && Num_triggers == 10 && xl_object_trigger_count() == 2 && std::strlen(xl_message_identity()) == 64, "Message startup did not install owned resources");
			xl_wall_trigger_frame();
			message_expect(12, 100, "Object autoplay");
			require((Triggers[4].flags & TF_DISABLED) && xl_wall_trigger(0, xl_compat::inspect_level(runtime_layout_read(ordinal == 1 ? "msg23.rl2" : "msg27.rl2")).object_trigger_records[0].object, 0) == 1, "Message actor gate or autoplay flags differ");
			message_expect(12, 100, "Object autoplay");
			check_trigger(&Segments[Walls[0].segnum], static_cast<short>(Walls[0].sidenum), static_cast<short>(Players[Player_num].objnum), 0);
			message_expect(1, 100, "Wall message\nSecond line");
			xl_trigger_state trigger_state{};
			require(xl_wall_trigger_state(0, &trigger_state) && trigger_state.delay_ms == -1 && trigger_state.operated, "Display seconds were misinterpreted as a trigger delay");
			require(check_trigger_sub(1, Player_num, 1) == 0 && (Triggers[1].flags & TF_DISABLED), "Message one-shot flags failed");
			message_expect(2, 3, "Replacement");
			require(check_trigger_sub(5, Player_num, 1) == 0, "Message master child failed");
			message_expect(1, 100, "Wall message\nSecond line");
			require(check_trigger_sub(6, Player_num, 1) == 0, "Inert message link fields became geometric dependencies");
			message_expect(7, 1000, "Long duration");
			check_trigger_sub(8, Player_num, 1);
			message_expect(7, 1000, "Long duration");
			check_trigger_sub(7, Player_num, 1);
			message_expect(0, 0, "");
			check_trigger_sub(2, Player_num, 1);
			message_expect(3, 1, "Short message");
			const xl_message_state near_expiry{ 950, 3, 1 };
			require(xl_message_restore(&near_expiry, 1) && xl_message_alpha() > 0 && xl_message_alpha() <= 0.1f, "Message fade-out phase differs");
			timer_delay(F1_0 / 8);
			xl_message_frame();
			message_expect(0, 0, "");
			int owner = -1;
			for (int i = 0; i < xl_object_trigger_count(); ++i) {
				xl_object_trigger_state state{};
				require(xl_object_trigger_state_get(i, &state), "Cannot read message event owner");
				if (state.value == 11 * F1_0) owner = state.owner;
			}
			require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Missing message removal owner");
			Objects[owner].flags |= OF_SHOULD_BE_DEAD;
			obj_delete_all_that_should_be_dead();
			message_expect(11, 100, "Collected object message");
			require(Objects[owner].type == OBJ_NONE, "Message test did not use actual object deletion");
			check_trigger_sub(9, Player_num, 1);
			message_expect(10, 100, "Repeat message");
			const auto identity = std::string(xl_message_identity());
			for (const auto &bad : std::array<std::array<const char *, 2>, 4>{ { { "nomsg.rl2", "Missing XL message table" }, { "badmsg.rl2", "Duplicate XL level text ID" }, { "badid.rl2", "is missing from" }, { "badsnd.rl2", "Missing XL named-audio resource" } } }) {
				require(!xl_level_supported(bad[0], 0) && std::strstr(xl_level_rejection(), bad[1]) && load_level(bad[0]) != 0, "Message dependency rejection missing or unhelpful");
				message_expect(10, 100, "Repeat message");
				require(identity == xl_message_identity(), "Rejected level replaced active message resources");
			}
			const auto shadow = root / "message-shadow";
			std::filesystem::create_directories(shadow);
			const auto filename = ordinal == 1 ? "msg23.rl2" : "msg27.rl2";
			const auto message_name = ordinal == 1 ? "msg23.msg" : "msg27.msg";
			write_bytes((shadow / message_name).string().c_str(), { '1', ':', 'x' });
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount message ownership negative control");
			const char *resolved = PHYSFS_getRealDir(message_name);
			require(resolved && std::filesystem::weakly_canonical(resolved) == std::filesystem::weakly_canonical(shadow), "Message negative control did not shadow the selected resource");
			require(!xl_level_supported(filename, 0) && std::strstr(xl_level_rejection(), "shadowed outside the selected mission HOG"), "Unowned message file was silently accepted");
			message_expect(10, 100, "Repeat message");
			require(identity == xl_message_identity() && PHYSFS_removeFromSearchPath(shadow.string().c_str()) && xl_level_supported(filename, 0), "Message ownership rejection did not recover without mutation");
			message_bad_state(root);
			char save[] = "message-earned.sav", description[] = "XL message phase";
			stop_time();
			const auto earned = message_snapshot();
			require(state_save_all_sub(save, description), "Cannot save earned message phase");
			require(load_base_mission("xchanged", error, sizeof(error)) && !load_level("msg23.rl2"), "Changed message resource control failed");
			auto *file = PHYSFS_openRead("message-state.bin");
			require(identity != xl_message_identity() && file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && !message_snapshot().id, "Foreign MSG resource accepted saved message state");
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Message stock transition failed");
			require(!*xl_message_identity() && !*xl_message_text() && !message_snapshot().id && xl_message_alpha() == 0, "Owned message leaked into Redux");
			trigger control{};
			control.type = 29;
			control.value = F1_0;
			control.time = 100;
			require(xl_message_action(&control) == 0, "Message path intercepted stock gameplay");
			require(state_restore_all_sub(save, 0) && identity == xl_message_identity(), "Disk-through-stock message restore failed");
			message_expect(10, 100, "Repeat message");
			const auto restored = message_snapshot();
			require(restored.elapsed_ms >= earned.elapsed_ms && restored.elapsed_ms < earned.elapsed_ms + 2000, "Disk restore restarted or consumed message duration");
			start_time();
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 messages: wall/master/object/autoplay dispatch, duration, expiry, owned rejection and disk-through-stock passed\n";
		}
		std::cout << "XL message runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL message runtime failed: " << error.what() << '\n';
		return 1;
	}
}
