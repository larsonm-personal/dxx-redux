// Exercise the actual engine station arrays and native mode wrappers
static void test_matcen_stations(const char *output)
{
	nlohmann::json trace = nlohmann::json::array();
#ifdef __ANDROID__
	const int routes = 4;
#else
	const int routes = 3;
#endif
	for (int restore = 0; restore != routes; ++restore)
		for (int old_mode = 0; old_mode != MATCEN_MODE_COUNT; ++old_mode)
			for (int mode = 0; mode != MATCEN_MODE_COUNT; ++mode) {
				matcen_mode_reset_game();
				require(matcen_mode_set(old_mode), "set initial policy");
				matcen_mode_record_activation(0);
				matcen_mode_record_activation(1);
				matcen_mode_record_activation(1);
				Num_fuelcenters = 4;
				Num_robot_centers = 2;
				std::memset(Station, 0, sizeof(Station));
				for (int i = 0; i != Num_fuelcenters; ++i) {
					Station[i].Type = i % 2 ? SEGMENT_IS_ROBOTMAKER : SEGMENT_IS_FUELCEN;
					Station[i].segnum = i;
					Station[i].Enabled = 1;
					Station[i].Lives = 3;
					Station[i].Capacity = (i + 1) * F1_0;
					Station[i].MaxCapacity = i % 2 ? 100 * F1_0 : Station[i].Capacity;
					Station[i].Timer = (i + 5) * F1_0;
					Station[i].Disable_time = (i + 10) * F1_0;
				}
				RobotCenters[0].fuelcen_num = 1;
				RobotCenters[1].fuelcen_num = 3;
				FuelCenter expected[MAX_NUM_FUELCENS];
				std::memcpy(expected, Station, sizeof(expected));
				const bool shutdown = old_mode == MATCEN_MODE_PAUSED || mode == MATCEN_MODE_PAUSED;
				if (shutdown)
					for (int i : { 1, 3 }) {
						expected[i].Enabled = 0;
						expected[i].Disable_time = 0;
					}
				const uint8_t saved_counts[] = { 3, 4 };
#ifdef __ANDROID__
				if (restore == 3) {
					android_save_meta_write_params params = {};
#ifdef DXX_BUILD_DESCENT_II
					params.game_id = ANDROID_SAVE_META_GAME_D2;
#else
					params.game_id = ANDROID_SAVE_META_GAME_D1;
#endif
					params.matcen_mode = static_cast<uint8_t>(mode);
					params.matcen_activation_counts[0] = saved_counts[0];
					params.matcen_activation_counts[1] = saved_counts[1];
					PHYSFS_file *saved = PHYSFS_openWrite("matcen-save-meta.bin");
					require(saved != nullptr, "open actual metadata save file");
					require(android_save_meta_write_physfs(saved, &params), "write native metadata trailer");
					require(PHYSFS_close(saved), "close saved metadata");
					android_save_meta_disk meta = {};
					require(android_save_meta_read_path("matcen-save-meta.bin", &meta), "read native saved metadata");
					state_android_restore_matcen_mode_from_meta(&meta);
				} else
#endif
				    if (restore == 2) {
					Game_mode = GM_MULTI | GM_MULTI_COOP;
					Player_num = 1;
					Multi_master_playernum = 0;
					N_players = 2;
					ubyte packet[3 + MATCEN_MODE_MAX_CENTERS] = {};
					packet[0] = MULTI_MATCEN_MODE;
					packet[1] = 1; // Native MATCEN_MODE_STATE wire discriminator
					packet[2] = static_cast<ubyte>(mode);
					packet[3] = saved_counts[0];
					packet[4] = saved_counts[1];
					multi_do_matcen_mode(packet, Multi_master_playernum);
					Game_mode = 0;
				} else
					require(restore ? matcen_restore_mode(mode, saved_counts, 2) : matcen_set_mode(mode), "accept native mode transition");
				require(std::memcmp(expected, Station, sizeof(expected)) == 0, "shutdown changes only actual robot Enabled/Disable_time fields");
				if (shutdown) {
					FrameTime = F1_0 / 60;
					Game_suspended = 0;
					fuelcen_update_all();
					require(std::memcmp(expected, Station, sizeof(expected)) == 0, "actual station update leaves stopped robot centers inert");
				}
				require(matcen_mode_get() == mode, "native wrapper applies requested mode");
				uint8_t counts[MATCEN_MODE_MAX_CENTERS] = {};
				matcen_mode_get_activation_counts(counts, sizeof(counts));
				require(counts[0] == (restore ? 3 : 1) && counts[1] == (restore ? 4 : 2), "set retains counts and restore applies saved counts");
				for (int i = 2; i != MATCEN_MODE_MAX_CENTERS; ++i)
					require(counts[i] == 0, "unused center counts remain zero");
				require(matcen_mode_can_activate(0) == (mode == MATCEN_MODE_DEFAULT), "used center follows default/one-round/paused policy");
				require(matcen_mode_can_activate(2) == (mode != MATCEN_MODE_PAUSED), "unused center follows mode policy");
				trace.push_back({ { "route", restore }, { "old_mode", old_mode }, { "mode", mode }, { "shutdown", shutdown }, { "late_enabled", Station[3].Enabled } });
			}
	FILE *file = std::fopen(output, "wb");
	require(file != nullptr, "open matcen station trace");
	const std::string text = trace.dump(2) + "\n";
	require(std::fwrite(text.data(), 1, text.size(), file) == text.size(), "write matcen station trace");
	require(std::fclose(file) == 0, "close matcen station trace");
}
