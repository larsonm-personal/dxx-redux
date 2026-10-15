// Synthetic inventory and identity edge cases on public data; real fire/release/save/restore
static void exercise_guided_save_runtime(const char *scenario)
{
#ifdef DXX_BUILD_DESCENT_II
	char filename[] = "guided-runtime.sav", description[21] = "Guided runtime";
	if (std::strcmp(scenario, "load-active") == 0 || std::strcmp(scenario, "load-released") == 0) {
		const bool active = std::strcmp(scenario, "load-active") == 0;
		require(Guided_missile[Player_num] == nullptr, "new process has no active guided pointer");
		require(state_restore_all_sub(filename, 0), "separate-process ordinary restore");
		require((Guided_missile[Player_num] != nullptr) == active, "separate-process guided ownership");
		const int ammo = Players[Player_num].secondary_ammo[GUIDED_INDEX];
		do_missile_firing(0);
		require(Players[Player_num].secondary_ammo[GUIDED_INDEX] == ammo - (active ? 0 : 1), "separate-process next real fire");
		std::printf("PASS guided save scenario=%s separate-process restore and next real fire\n", scenario);
		return;
	}
	Players[Player_num].secondary_weapon_flags |= 1 << GUIDED_INDEX;
	Players[Player_num].secondary_ammo[GUIDED_INDEX] = 8;
	Players[Player_num].secondary_weapon = GUIDED_INDEX;
	const bool format = std::strcmp(scenario, "format") == 0;
	const bool no_guided = std::strcmp(scenario, "none") == 0 || format;
	const bool active = std::strcmp(scenario, "active") == 0;
	const bool released = std::strcmp(scenario, "released") == 0;
	const bool multiple = std::strcmp(scenario, "multiple") == 0;
	const bool stale = std::strcmp(scenario, "stale") == 0;
	const bool parent = std::strcmp(scenario, "parent") == 0;
	require(no_guided || active || released || multiple || stale || parent, "known guided save scenario");
	int slot = -1, signature = -1;
	if (!no_guided) {
		do_missile_firing(0);
		require(Guided_missile[Player_num] != nullptr, "real guided launch");
		slot = Guided_missile[Player_num] - Objects;
		signature = Objects[slot].signature;
		if (released || multiple) {
			do_missile_firing(0);
			require(Guided_missile[Player_num] == nullptr && Objects[slot].type == OBJ_WEAPON, "real release leaves live missile");
		}
		if (multiple) {
			do_missile_firing(0);
			require(Guided_missile[Player_num] != nullptr && Guided_missile[Player_num] != &Objects[slot], "second live guided missile");
			// Select the earlier valid identity to ensure restore does not choose the last live missile
			Guided_missile[Player_num] = &Objects[slot];
			Guided_missile_sig[Player_num] = signature;
		}
		if (stale) ++Guided_missile_sig[Player_num];
		if (parent) ++Objects[slot].ctype.laser_info.parent_signature;
	}
	const bool expected_active = active || multiple;
	stop_time();
	require(state_save_all_sub(filename, description), "ordinary guided save");
	const int saved_ammo = Players[Player_num].secondary_ammo[GUIDED_INDEX];
	// Invalid identities are intentionally normalized by the writer; compare real fire for valid states
	int expected_ammo = saved_ammo - (expected_active ? 0 : 1);
	if (!stale && !parent) {
		do_missile_firing(0);
		require(Players[Player_num].secondary_ammo[GUIDED_INDEX] == expected_ammo, "uninterrupted next real fire");
	}
	for (int restore = 0; restore < 2; ++restore) {
		// First restore has a changed warm pointer; second has fresh-process-like empty pointer state
		if (restore) for (int i = 0; i < MAX_PLAYERS; ++i) {
			Guided_missile[i] = nullptr;
			Guided_missile_sig[i] = -1;
		}
		require(state_restore_all_sub(filename, 0), "ordinary guided restore");
		require((Guided_missile[Player_num] != nullptr) == expected_active, "restore explicit guided ownership");
		if (expected_active)
			require(Guided_missile[Player_num] == &Objects[slot] && Guided_missile_sig[Player_num] == signature,
			        "restore exact guided identity among live missiles");
		do_missile_firing(0);
		require(Players[Player_num].secondary_ammo[GUIDED_INDEX] == expected_ammo, "resumed next real fire matches ownership");
	}
	if (format) {
		PHYSFS_file *file = PHYSFSX_openReadBuffered(filename);
		require(file != nullptr, "open saved guided extension");
		bytes original(static_cast<size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_read(file, original.data(), 1, original.size()) == original.size() && PHYSFS_close(file), "read saved guided extension");
		require(original.size() > (MAX_PLAYERS * 8 + 4) && original[4] == 45, "native D2 guided format version");
		bytes malformed = original;
		set_int(malformed, malformed.size() - (MAX_PLAYERS * 8 + 4), MAX_OBJECTS);
		write_fixture(filename, malformed);
		require(!state_restore_all_sub(filename, 0), "reject out-of-range guided slot");
		malformed = original;
		malformed.pop_back();
		write_fixture(filename, malformed);
		require(!state_restore_all_sub(filename, 0), "reject truncated runtime tail");
		malformed = original;
		malformed.resize(malformed.size() - 5);
		write_fixture(filename, malformed);
		require(!state_restore_all_sub(filename, 0), "reject truncated guided identity before phase");
		malformed = original;
		set_int(malformed, malformed.size() - 4, 65536);
		write_fixture(filename, malformed);
		require(!state_restore_all_sub(filename, 0), "reject out-of-range full-save phase");
		malformed = original;
		set_int(malformed, malformed.size() - (MAX_PLAYERS * 8 + 4) + 4, 123);
		write_fixture(filename, malformed);
		require(!state_restore_all_sub(filename, 0), "reject noncanonical absent identity");
		// Native 41 has the same prefix and no guided ownership or wiggle phase tail
		bytes legacy = original;
		set_int(legacy, 4, 41);
		legacy.resize(legacy.size() - (MAX_PLAYERS * 8 + 4));
		write_fixture(filename, legacy);
		require(state_restore_all_sub(filename, 0), "retain ordinary native version41 save support");
		write_fixture(filename, original);
		require(state_restore_all_sub(filename, 0), "valid new save recovers after malformed inputs");
	}
	std::printf("PASS guided save scenario=%s warm/empty-pointer restore and next real fire\n", scenario);
#else
	(void)scenario;
	require(false, "guided save fixture requires D2 engine");
#endif
}
