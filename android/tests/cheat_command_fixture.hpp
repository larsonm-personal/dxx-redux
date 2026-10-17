#include "game_cheat.h"

// Runs after native keyboard capture, against the same initialized real engine
static void test_cheat_command_isolation(const char *code, const input_demo_recorder_settings &settings)
{
	char error[256] = "";
	const auto saved_config = PlayerCfg;
	const auto saved_controls = Controls;
	const auto saved_player = Players[Player_num];
	const auto saved_cheats = cheats;
	const int saved_rear = Rear_view;
	unsigned char saved_keys[256];
	std::memcpy(saved_keys, (const void *) keyd_pressed, sizeof(saved_keys));
	for (int rear : { 0, 1 }) {
		for (int sticky : { 0, 1 }) {
			cheats = {};
			Rear_view = rear;
			PlayerCfg.StickyRearview = sticky;
			PlayerCfg.CurrentCockpitMode = rear ? CM_REAR_VIEW : CM_FULL_COCKPIT;
			PlayerCfg.PreferredCockpitMode = CM_FULL_COCKPIT;
			// Deliberately conflicting bindings and held controls/modifiers
			std::memset(PlayerCfg.KeySettings[0], KEY_B, sizeof(PlayerCfg.KeySettings[0]));
			kc_set_controls();
			Controls = {};
			Controls.rear_view_state = rear;
			Controls.fire_primary_state = 1;
			Controls.fire_secondary_state = 1;
			Controls.forward_thrust_time = F1_0;
			keyd_pressed[KEY_R] = keyd_pressed[KEY_LSHIFT] = keyd_pressed[KEY_LCTRL] = 1;
			const auto config = PlayerCfg;
			const auto controls = Controls;
			unsigned char keys[256];
			std::memcpy(keys, (const void *) keyd_pressed, sizeof(keys));
			for (int repeat = 0; repeat < 2; ++repeat) {
				Players[Player_num].energy = F1_0;
				Players[Player_num].score = 123;
				require(game_execute_cheat(code), "complete menu weapon code accepted");
				require(cheats.enabled && Players[Player_num].energy == MAX_ENERGY && Players[Player_num].score == 0,
				        "menu grants and repeated refills preserve native cheat effects");
				require(Rear_view == rear && !std::memcmp(&config, &PlayerCfg, sizeof(config)) &&
				            !std::memcmp(&controls, &Controls, sizeof(controls)) &&
				            !std::memcmp(keys, (const void *) keyd_pressed, sizeof(keys)),
				        "menu cheat preserves bindings, cockpit, sticky rear view, controls and held keys");
			}
		}
	}
	cheats = {};
	const auto rejected_player = Players[Player_num];
	for (const char *invalid : { "", "big", "prefixbigred", "bigredhonestbob", "unknown" })
		require(!game_execute_cheat(invalid), "only an exact native cheat name is accepted");
	require(!cheats.enabled && !std::memcmp(&rejected_player, &Players[Player_num], sizeof(rejected_player)), "invalid codes do not enable cheats or change inventory");
	for (int blocked = 0; blocked < 4; ++blocked) {
		Game_mode = blocked == 0 ? GM_MULTI : 0;
		Player_is_dead = blocked == 1;
		Endlevel_sequence = blocked == 2;
		Newdemo_state = blocked == 3 ? ND_STATE_PLAYBACK : ND_STATE_NORMAL;
		require(!game_execute_cheat(code), "menu cheats reject multiplayer, death, endlevel and playback");
	}
	Game_mode = Player_is_dead = Endlevel_sequence = 0;
	Newdemo_state = ND_STATE_NORMAL;
	PlayerCfg = saved_config;
	kc_set_controls();
	Controls = saved_controls;
	Rear_view = saved_rear;
	std::memcpy((void *) keyd_pressed, saved_keys, sizeof(saved_keys));
	require(input_demo_recorder_start(&settings, error, sizeof(error)), error);
	require(game_execute_cheat(code) && input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() > 0,
	        "menu weapon grant uses ordered semantic recording");
#ifdef DXX_BUILD_DESCENT_II
	const char *unsupported = "almighty";
#else
	const char *unsupported = "racerx";
#endif
	const int flags = Players[Player_num].flags;
	require(game_execute_cheat(unsupported) && !input_demo_recorder_is_active() &&
	            ((flags ^ Players[Player_num].flags) & PLAYER_FLAGS_INVULNERABLE),
	        "unsupported menu cheat cancels capture and still applies its native effect");
	Players[Player_num] = saved_player;
	cheats = saved_cheats;
	std::puts("PASS: complete cheat commands preserve ship input/settings and recording policy");
}
