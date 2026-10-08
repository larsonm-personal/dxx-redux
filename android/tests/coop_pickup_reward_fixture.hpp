// Exercise duplicate reward admission through the actual native pickup dispatcher
static void test_coop_pickup_rewards(const char *output)
{
	using nlohmann::json;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	Newdemo_state = ND_STATE_NORMAL;
	Player_is_dead = 0;
	Difficulty_level = 2;
	InitWeaponOrdering();
	for (int i = 0; i < N_TEXT_STRINGS; ++i) Text_string[i] = const_cast<char *>("fixture");
	struct reward_case {
		int id, weapon, laser, flag;
	};
	std::vector<reward_case> items = {
		{ POW_SPREADFIRE_WEAPON, SPREADFIRE_INDEX, -1, 0 },
		{ POW_VULCAN_WEAPON, VULCAN_INDEX, -1, 0 },
		{ POW_LASER, -1, MAX_LASER_LEVEL, 0 },
		{ POW_QUAD_FIRE, -1, -1, PLAYER_FLAGS_QUAD_LASERS },
	};
#ifdef DXX_BUILD_DESCENT_II
	items.push_back({ POW_PHOENIX_WEAPON, PHOENIX_INDEX, -1, 0 });
	items.push_back({ POW_SUPER_LASER, -1, MAX_SUPER_LASER_LEVEL, 0 });
	items.push_back({ POW_FULL_MAP, -1, -1, PLAYER_FLAGS_MAP_ALL });
	Mission mission = {};
#endif
	json trace = json::array();
	const int multiplayer = GM_MULTI & ~GM_NETWORK;
	for (int edition : { 1, 2 }) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = edition;
		Current_mission = &mission;
#else
		if (edition == 2) continue;
#endif
		for (const auto &item : items)
			for (int mode : { 0, multiplayer, multiplayer | GM_MULTI_COOP })
				for (int host_observer : { 0, 1 })
					for (int subject : { 0, 2 })
						for (int phase = CONNECT_DISCONNECTED; phase <= CONNECT_END_MENU; ++phase)
							for (int missing : { 0, 1 }) {
#ifdef DXX_BUILD_DESCENT_II
								if (edition == 1 && (item.weapon > FUSION_INDEX || item.laser > MAX_LASER_LEVEL || item.id == POW_FULL_MAP)) continue;
#endif
								Game_mode = mode;
								Player_num = 1;
								N_players = 3;
								Netgame.host_is_obs = static_cast<ubyte>(host_observer);
								Netgame.GaussAmmoStyle = GAUSS_STYLE_DUPLICATING;
								ConsoleObject = &Objects[0];
								ConsoleObject->type = OBJ_PLAYER;
								for (int i = 0; i < N_players; ++i) {
									Players[i] = {};
									Players[i].connected = CONNECT_PLAYING;
									Players[i].shields = Players[i].energy = 100 * F1_0;
									Players[i].lives = 3;
									Players[i].primary_weapon_flags = (1 << MAX_PRIMARY_WEAPONS) - 1;
									Players[i].laser_level = static_cast<decltype(Players[i].laser_level)>(item.laser < 0 ? MAX_LASER_LEVEL : item.laser);
									Players[i].flags = item.flag;
								}
								Players[subject].connected = static_cast<decltype(Players[subject].connected)>(phase);
								if (missing) {
									if (item.weapon >= 0) Players[subject].primary_weapon_flags &= ~(1 << item.weapon);
									if (item.laser >= 0) --Players[subject].laser_level;
									Players[subject].flags = 0;
								}
								const bool excluded = phase == CONNECT_DISCONNECTED || (subject == 0 && host_observer);
								const bool reward = mode == 0 || ((mode & GM_MULTI_COOP) && (!missing || excluded));
								Powerup_info[item.id].hit_sound = -1;
								object pickup = {};
								pickup.type = OBJ_POWERUP;
								pickup.id = static_cast<decltype(pickup.id)>(item.id);
								pickup.ctype.powerup_info.count = 2 * VULCAN_AMMO_AMOUNT;
								const unsigned sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
								const int used = do_powerup(&pickup);
								require((used != 0) == reward, "native duplicate pickup retains the reward/consumption contract");
								if (item.weapon != VULCAN_INDEX)
									require(Players[1].energy == (reward ? 112 : 100) * F1_0, "duplicate energy depends on every included peer's inventory");
								require(sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX), "duplicate rewards do not consume SIM or FX RNG");
								trace.push_back({ { "case", { edition, item.id, mode, host_observer, subject, phase, missing } },
								                  { "used", used },
								                  { "energy", Players[1].energy },
								                  { "ammo", Players[1].primary_ammo[VULCAN_INDEX] },
								                  { "remaining_ammo", pickup.ctype.powerup_info.count },
								                  { "score", Players[1].score },
								                  { "flags", Players[1].flags },
								                  { "weapons", Players[1].primary_weapon_flags },
								                  { "laser", Players[1].laser_level } });
							}
	}
	Current_mission = nullptr;
	std::FILE *file = std::fopen(output, "wb");
	require(file != nullptr, "open native reward trace");
	const std::string text = trace.dump(2) + "\n";
	require(std::fwrite(text.data(), 1, text.size(), file) == text.size() && std::fclose(file) == 0, "write native reward trace");
	std::printf("PASS: %zu native duplicate pickup reward cases\n", trace.size());
}
