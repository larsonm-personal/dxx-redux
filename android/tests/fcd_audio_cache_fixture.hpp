// Actual D2 audio/gameplay queries, including the original cache's historical quirks
extern "C" {
void digi_get_sound_loc(vms_matrix *, vms_vector *, int, vms_vector *, int, fix, int *, int *, fix);
void flush_fcd_cache(void);
void add_to_fcd_cache(int, int, int, fix);
void blast_nearby_glass(object *, fix);
extern int Connected_segment_distance;
}

static void fcd_test_scene(int cells = 16)
{
	init_test_corridor(cells);
	Game_mode = 0;
	Player_num = 0;
	Newdemo_state = ND_STATE_NORMAL;
	GameArg.SndNoSound = 1;
	flush_fcd_cache();
}

static fix fcd_game_query(int first, int last, fix offset = 0, int depth = 32, int flags = WID_FLY_FLAG)
{
	vms_vector p0, p1;
	compute_segment_center(&p0, &Segments[first]);
	compute_segment_center(&p1, &Segments[last]);
	p1.z += offset;
	return find_connected_distance(&p0, first, &p1, last, depth, flags);
}

static int fcd_sound_query(int first, int last, fix listener_offset = 0)
{
	vms_vector p0, p1;
	vms_matrix orient = vmd_identity_matrix;
	compute_segment_center(&p0, &Segments[first]);
	compute_segment_center(&p1, &Segments[last]);
	p0.z += listener_offset;
	int volume, pan;
	digi_get_sound_loc(&orient, &p0, first, &p1, last, F1_0, &volume, &pan, 512 * F1_0);
	return volume;
}

static int fcd_expected_volume(fix distance)
{
	return F1_0 - fixdiv(distance, 640 * F1_0);
}

static nlohmann::json fcd_gameplay_cache_sequence()
{
	fcd_test_scene();
	GameTime64 = 10 * F1_0;
	auto rows = nlohmann::json::array();
	auto check = [&](const char *name, fix actual, fix expected, int segments) {
		require(actual == expected && Connected_segment_distance == segments, name);
		rows.push_back({ { "case", name }, { "distance", actual }, { "segments", Connected_segment_distance } });
	};
	check("same segment", fcd_game_query(0, 0, 5 * F1_0), 5 * F1_0, 0);
	check("adjacent segment", fcd_game_query(0, 1), 20 * F1_0, 1);
	check("fresh path", fcd_game_query(0, 5), 100 * F1_0, 6);
	check("historical segment-only key", fcd_game_query(0, 5, 5 * F1_0, 2, 0), 100 * F1_0, 6);
	flush_fcd_cache();
	add_to_fcd_cache(0, 5, 41, 80 * F1_0);
	check("80 is not admitted", fcd_game_query(0, 5), 100 * F1_0, 6);
	flush_fcd_cache();
	add_to_fcd_cache(0, 5, 42, 80 * F1_0 + 1);
	check("80 plus one is admitted", fcd_game_query(0, 5), 80 * F1_0 + 1, 42);
	flush_fcd_cache();
	for (int i = 0; i < 8; ++i) add_to_fcd_cache(0, 5 + i, 11 + i, (111 + i) * F1_0);
	add_to_fcd_cache(0, 6, 0, 80 * F1_0);
	check("historical removal retains matching slot", fcd_game_query(0, 6), 112 * F1_0, 12);
	check("historical removal invalidates current slot", fcd_game_query(0, 5), 100 * F1_0, 6);
	flush_fcd_cache();
	for (int i = 0; i < 9; ++i) add_to_fcd_cache(0, 5 + i, 21 + i, (201 + i) * F1_0);
	check("ninth insertion retains second slot", fcd_game_query(0, 6), 202 * F1_0, 22);
	check("ninth insertion replaces first slot", fcd_game_query(0, 5), 100 * F1_0, 6);
	GameTime64 = 20 * F1_0;
	(void) fcd_game_query(0, 5);
	add_to_fcd_cache(0, 6, 77, 777 * F1_0);
	GameTime64 = 22 * F1_0;
	check("exactly two seconds remains cached", fcd_game_query(0, 6), 777 * F1_0, 77);
	GameTime64++;
	check("more than two seconds expires", fcd_game_query(0, 6), 120 * F1_0, 7);
	GameTime64 = 21 * F1_0;
	check("time rewind expires", fcd_game_query(0, 6, 5 * F1_0), 125 * F1_0, 7);
	GameTime64 = 30 * F1_0;
	(void) fcd_game_query(0, 5);
	GameTime64 = 31 * F1_0;
	flush_fcd_cache();
	add_to_fcd_cache(0, 6, 44, 444 * F1_0);
	GameTime64 = 32 * F1_0 + 1;
	check("explicit flush retains expiry clock", fcd_game_query(0, 6), 120 * F1_0, 7);
	GameTime64 = 40 * F1_0;
	flush_fcd_cache();
	check("depth failure returns minus one", fcd_game_query(0, 5, 0, 3), -1, 1000);
	check("failure is cached as 1000", fcd_game_query(0, 5), 1000 * F1_0, 1000);
	flush_fcd_cache();
	check("blocked query returns minus one", fcd_game_query(0, 5, 0, 32, 0), -1, 1000);
	check("cached failure ignores new doorway flags", fcd_game_query(0, 5), 1000 * F1_0, 1000);
	for (int adjacent : { 0, 1 }) {
		GameTime64 = (50 + adjacent * 10) * F1_0;
		(void) fcd_game_query(0, 5);
		add_to_fcd_cache(0, 6, 33, 333 * F1_0);
		GameTime64 += 3 * F1_0;
		(void) fcd_game_query(0, adjacent);
		GameTime64 -= 2 * F1_0;
		check(adjacent ? "adjacent fast path skips expiry" : "same-segment fast path skips expiry", fcd_game_query(0, 6), 333 * F1_0, 33);
	}
	return rows;
}

static void test_fcd_audio_cache_domains()
{
	fcd_test_scene();
	GameTime64 = 100 * F1_0;
	require(fcd_sound_query(0, 5, -5 * F1_0) == fcd_expected_volume(105 * F1_0), "fresh actual audio distance");
	GameTime64 = 101 * F1_0;
	require(fcd_game_query(0, 5) == 100 * F1_0, "audio must not prime gameplay distance");
	add_to_fcd_cache(0, 6, 77, 111 * F1_0);
	require(fcd_game_query(0, 6) == 111 * F1_0, "gameplay sentinel cache hit");
	GameTime64 = 102 * F1_0 + 1;
	require(fcd_sound_query(0, 5, 5 * F1_0) == fcd_expected_volume(95 * F1_0), "audio cache expires locally");
	require(Connected_segment_distance == 77 && fcd_game_query(0, 6) == 111 * F1_0, "audio expiry preserves gameplay cache and distance result");
	for (int last = 5; last <= 13; ++last) (void) fcd_sound_query(0, last);
	require(Connected_segment_distance == 77 && fcd_game_query(0, 6) == 111 * F1_0, "audio eviction preserves gameplay cache and result");

	flush_fcd_cache();
	GameTime64 = 110 * F1_0;
	(void) fcd_game_query(0, 5);
	GameTime64 = 111 * F1_0;
	(void) fcd_sound_query(0, 5, -5 * F1_0);
	GameTime64 = 112 * F1_0 + 1;
	require(fcd_game_query(0, 5, 5 * F1_0) == 105 * F1_0, "gameplay cache expires locally");
	require(fcd_sound_query(0, 5, 5 * F1_0) == fcd_expected_volume(105 * F1_0), "gameplay expiry preserves audio cache");
	flush_fcd_cache();
	require(fcd_game_query(0, 5) == 100 * F1_0 && fcd_sound_query(0, 5, 5 * F1_0) == fcd_expected_volume(95 * F1_0), "explicit invalidation clears both caches");

	Connected_segment_distance = 123;
	(void) fcd_sound_query(0, 0, 5 * F1_0);
	(void) fcd_sound_query(0, 1);
	require(Connected_segment_distance == 123, "audio fast paths preserve gameplay distance result");
	flush_fcd_cache();
	Segments[4].children[4] = Segments[5].children[5] = -1;
	require(fcd_sound_query(0, 5) == 0 && Connected_segment_distance == 123, "failed audio path preserves gameplay distance result");
}

static nlohmann::json fcd_glass_case(bool audio, bool clear_after_audio)
{
	fcd_test_scene(5);
	for (int i = 0; i < Num_vertices; ++i) Vertices[i].z *= 2;
	validate_segment_all();
	GameTime64 = 200 * F1_0;
	Num_static_lights = 0;
	std::memset(Light_subtracted, 0, sizeof(Light_subtracted));
	object &player = Objects[0];
	player.segnum = 0;
	compute_segment_center(&player.pos, &Segments[0]);
	player.pos.z -= 19 * F1_0;
	player.orient = vmd_identity_matrix;
	ConsoleObject = Viewer = &player;
	static unsigned char pixels[4] = { 1, 1, 1, 1 };
	gr_init_bitmap(&GameBitmaps[1], BM_LINEAR, 0, 0, 2, 2, 2, pixels);
	Textures[1].index = 1;
	TmapInfo[1].eclip_num = 0;
	TmapInfo[1].destroyed = -1;
	std::memset(&Effects[0], 0, sizeof(Effects[0]));
	Effects[0].dest_bm_num = 2;
	Effects[0].dest_eclip = -1;
	Effects[0].dest_vclip = 3;
	Effects[0].sound_num = -1;
	Effects[0].dest_size = F1_0;
	Vclip[3].play_time = F1_0;
	Vclip[3].sound_num = -1;
	Segments[0].sides[0].tmap_num2 = 0x4001;
	vms_vector source;
	compute_segment_center(&source, &Segments[4]);
	const int slot = obj_create(OBJ_WEAPON, EARTHSHAKER_ID, 4, &source, &vmd_identity_matrix, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
	require(slot >= 0, "glass witness weapon allocation");
	object &weapon = Objects[slot];
	weapon.ctype.laser_info.parent_num = 0;
	weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
	weapon.ctype.laser_info.parent_signature = player.signature;
	d_srand(1234);
	unsigned int before = 0, after = 0;
	require(d_rand_get_state(&before), "glass witness reads SIM state");
	const auto calls = d_rand_get_call_count();
	if (audio) {
		int volume, pan;
		digi_get_sound_loc(&player.orient, &player.pos, 0, &source, 4, F1_0, &volume, &pan, 256 * F1_0);
		require(volume > 0, "glass witness exercises actual audible distance query");
	}
	if (clear_after_audio) flush_fcd_cache();
	blast_nearby_glass(&weapon, 340 * F1_0);
	object_runtime_state runtime = {};
	object_get_runtime_state(&runtime);
	require(d_rand_get_state(&after) && before == after && calls == d_rand_get_call_count(), "glass witness consumes no SIM randomness");
	return { { "overlay", static_cast<int>(Segments[0].sides[0].tmap_num2) }, { "light", Light_subtracted[0] },
		{ "objects", runtime.num_objects }, { "signature", runtime.signature_seed }, { "sim", after } };
}

static void test_fcd_audio_cache()
{
	Mission mission = {};
	mission.descent_version = 2;
	Current_mission = &mission;
	const auto expected = fcd_glass_case(false, false);
	require(expected["overlay"] == 0x4002 && expected["light"] == 1 && expected["signature"] == 2, "baseline blast destroys monitor and allocates explosion");
	for (const auto &mode : { std::pair<bool, bool>{ true, false }, { true, false }, { false, false }, { true, true }, { false, false } })
		require(fcd_glass_case(mode.first, mode.second) == expected, "actual positional audio must not change blast-glass gameplay");
	test_fcd_audio_cache_domains();
	std::puts(fcd_gameplay_cache_sequence().dump(2).c_str());
	Current_mission = nullptr;
	std::puts("PASS: D2 positional audio cache is isolated from gameplay");
}
