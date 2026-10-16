// Real engine functions, synthetic corridor, no replay compensation
#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern int weapon_box_user[2];
extern fix static_time[2];
void show_extra_views(void);
}
#endif

static void test_missile_camera_ownership()
{
#ifdef DXX_BUILD_DESCENT_II
	const auto saved_cfg = PlayerCfg;
	const int saved_boxes[] = { weapon_box_user[0], weapon_box_user[1] };
	const fix saved_static[] = { static_time[0], static_time[1] };
	const int saved_tick = d_tick_count, saved_demo = Newdemo_state;
	Newdemo_state = ND_STATE_NORMAL;
	unsigned cases = 0;
	for (int profile : { 2, 1 })
		for (int enabled : { 0, 1 })
			for (int draws : { 0, 1, 4 })
				for (int transition : { 0, 1, 2, 3, 4 }) {
					Mission mission = {};
					mission.descent_version = profile;
					Current_mission = &mission;
					object &robot = create_frame_robot(AIB_NORMAL, -5 * F1_0, 20);
					robot.pos = {};
					robot.ctype.ai_info.SUB_FLAGS = 0;
					Ai_local_info[&robot - Objects] = {};
					vms_vector far_position = { 0, 0, 150 * F1_0 }, near_position = { 0, 0, 10 * F1_0 };
					const int older = obj_create(OBJ_WEAPON, CONCUSSION_ID, 0, &far_position, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
					const int newer = obj_create(OBJ_WEAPON, GUIDEDMISS_ID, 0, &near_position, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
					require(older > 0 && newer > 0, "create both real missile identities");
					for (int i : { older, newer }) {
						auto &laser = Objects[i].ctype.laser_info;
						laser.parent_type = OBJ_PLAYER;
						laser.parent_num = 0;
						laser.parent_signature = ConsoleObject->signature;
						laser.creation_time = (i == older ? F1_0 : 2 * F1_0);
					}
					Missile_viewer = &Objects[older];
					Missile_viewer_sig = Objects[older].signature;
					Guided_missile[0] = &Objects[newer];
					Guided_missile_sig[0] = Objects[newer].signature;
					// Active guided, released guided, deleted selected, reused slot, stale guided
					if (transition >= 1 && transition <= 3) {
						release_guided_missile(0);
						require(Missile_viewer == &Objects[newer] && Missile_viewer_sig == Objects[newer].signature,
						        "guided release binds its own signature despite an existing ordinary selection");
					}
					if (transition == 2) obj_delete(newer);
					if (transition == 3) {
						obj_delete(newer);
						const int reused = obj_create(OBJ_WEAPON, CONCUSSION_ID, 0, &near_position, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
						require(reused == newer, "reuse selected slot with a different signature");
						Objects[reused].ctype.laser_info = Objects[older].ctype.laser_info;
						Objects[reused].ctype.laser_info.creation_time = 3 * F1_0;
					}
					if (transition == 4) ++Guided_missile_sig[0];
					PlayerCfg.MissileViewEnabled = enabled;
					Ai_last_missile_camera = -1;
					const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
					object *const selected = Missile_viewer, *const guided = Guided_missile[0];
					const int signature = Missile_viewer_sig, guided_signature = Guided_missile_sig[0];
					for (int draw = 0; draw < draws; ++draw) {
						PlayerCfg.MissileViewEnabled = (draw & 1) ? !enabled : enabled;
						select_missile_viewer();
						require(Missile_viewer == selected && Missile_viewer_sig == signature && Guided_missile[0] == guided &&
						            Guided_missile_sig[0] == guided_signature && Ai_last_missile_camera == -1,
						        "presentation toggles never mutate simulation camera identities");
					}
					for (int tick = 0; tick < 4; ++tick) {
						d_tick_count = tick;
						wake_up_missile_camera_robots();
					}
					const bool near_camera = transition < 2;
					require(Ai_last_missile_camera == (near_camera ? newer : older), "preserve guided/current/oldest resolver order and stale rejection");
					const bool awake = profile == 2 && near_camera;
					require(Ai_local_info[&robot - Objects].player_awareness_type == (awake ? PA_WEAPON_ROBOT_COLLISION : 0) &&
					            robot.ctype.ai_info.SUB_FLAGS == (awake ? SUB_FLAGS_CAMERA_AWAKE : 0),
					        "real visibility and awareness are independent of presentation, with native D1 exclusion");
					if (awake) {
						const int count = Highest_object_index;
						const ai_local local = Ai_local_info[&robot - Objects];
						ai_fire_laser_at_player(&robot, &robot.pos, 0, &ConsoleObject->pos);
						require(Highest_object_index == count && !std::memcmp(&local, &Ai_local_info[&robot - Objects], sizeof(local)),
						        "camera-only awareness still suppresses actual AI firing");
					}
					require(sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX), "camera ownership consumes no RNG");
					++cases;
				}
	// A hidden/disabled feed retires only presentation ownership, once
	PlayerCfg.MissileViewEnabled = 0;
	PlayerCfg.GuidedInBigWindow = 0;
	PlayerCfg.Cockpit3DView[0] = PlayerCfg.Cockpit3DView[1] = CV_NONE;
	Guided_missile[0] = &Objects[2];
	Guided_missile_sig[0] = -123;
	object *const guided = Guided_missile[0], *const selected = Missile_viewer;
	const int signature = Missile_viewer_sig, last = Ai_last_missile_camera;
	for (int previous : { WBU_GUIDED, WBU_MISSILE }) {
		weapon_box_user[0] = WBU_WEAPON;
		weapon_box_user[1] = previous;
		show_extra_views();
		require(weapon_box_user[1] == WBU_STATIC, "ended or disabled displayed feed transitions to static");
		static_time[1] = F1_0 / 4;
		show_extra_views();
		require(weapon_box_user[1] == WBU_STATIC && static_time[1] == F1_0 / 4, "repeated presentation does not restart static");
		weapon_box_user[1] = WBU_WEAPON; // Static animation has finished
		show_extra_views();
		require(weapon_box_user[1] == WBU_WEAPON, "disabled live missile cannot restart finished static");
		require(Guided_missile[0] == guided && Missile_viewer == selected && Missile_viewer_sig == signature && Ai_last_missile_camera == last,
		        "actual extra-view dispatch never retires gameplay identity");
	}
	// Invalid guided release must not adopt a reused slot or publish an end event
	Guided_missile[0] = &Objects[2];
	Guided_missile_sig[0] = Objects[2].signature + 1;
	release_guided_missile(0);
	require(!Guided_missile[0] && Missile_viewer == selected && Missile_viewer_sig == signature,
	        "stale guided release preserves selected missile identity");
	Mission creation_mission = {};
	creation_mission.descent_version = 2;
	Current_mission = &creation_mission;
	create_frame_robot(AIB_NORMAL, -5 * F1_0, 20);
	const auto saved_weapon = Weapon_info[CONCUSSION_ID];
	const int saved_weapon_count = N_weapon_types;
	player_ship ship = {};
	player_ship *const saved_ship = Player_ship;
	Player_ship = &ship;
	auto &weapon = Weapon_info[CONCUSSION_ID];
	weapon = {};
	weapon.render_type = WEAPON_RENDER_NONE;
	weapon.flash_sound = weapon.flash_vclip = -1;
	weapon.mass = F1_0;
	weapon.speedvar = 128;
	weapon.lifetime = 10 * F1_0;
	weapon.speed[Difficulty_level] = 40 * F1_0;
	weapon.children = -1;
	if (N_weapon_types <= CONCUSSION_ID) N_weapon_types = CONCUSSION_ID + 1;
	Missile_viewer = Guided_missile[0] = nullptr;
	Missile_viewer_sig = -999;
	Laser_player_fire(ConsoleObject, CONCUSSION_ID, 0, 0, 0, ConsoleObject->orient.fvec);
	require(Missile_viewer && Missile_viewer->id == CONCUSSION_ID && Missile_viewer_sig == Missile_viewer->signature,
	        "actual ordinary firing binds the new camera signature before any renderer or resolver");
	Player_ship = saved_ship;
	Weapon_info[CONCUSSION_ID] = saved_weapon;
	N_weapon_types = saved_weapon_count;
	Missile_viewer = Guided_missile[0] = nullptr;
	Missile_viewer_sig = Guided_missile_sig[0] = Ai_last_missile_camera = -1;
	Current_mission = nullptr;
	PlayerCfg = saved_cfg;
	for (int window = 0; window < 2; ++window) {
		weapon_box_user[window] = saved_boxes[window];
		static_time[window] = saved_static[window];
	}
	d_tick_count = saved_tick;
	Newdemo_state = saved_demo;
	std::printf("PASS: missile camera ownership %u real resolver cases and static transitions\n", cases);
#endif
}
