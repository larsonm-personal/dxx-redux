#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <physfs.h>
#include <SDL.h>

extern "C" {
#include "args.h"
#include "bm.h"
#include "config.h"
#include "console.h"
#include "cntrlcen.h"
#include "digi.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "gr.h"
#include "inferno.h"
#include "messagebox.h"
#include "mission.h"
#include "multi.h"
#include "object.h"
#include "physfsx.h"
#include "player.h"
#include "powerup.h"
#include "screens.h"
#include "secret_area_scan.h"
#include "secretarea.h"
#include "songs.h"
#ifdef DXX_BUILD_DESCENT_II
#include "switch.h"
#endif
#include "texmerge.h"
#include "text.h"
#include "u_mem.h"
#include "wall.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif
extern "C" void gameseq_init_network_players(void);

static unsigned char *headless_screen_pixels = NULL;
static int secret_area_dump_failed = 0;
static int secret_area_missing_secret_levels = 0;
static int route_shadow_strict = 0;

static std::string dump_metadata_json(const nlohmann::ordered_json &value)
{
	return value.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

static void trace_dump_init(const char *stage)
{
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
		fprintf(stderr, "SECRET-AREA-DUMP TRACE %s\n", stage);
		fflush(stderr);
	}
}

static const char *find_arg_value(int argc, char *argv[], const char *name)
{
	for (int index = 1; index + 1 < argc; ++index)
		if (!strcmp(argv[index], name))
			return argv[index + 1];
	return NULL;
}

static int has_arg(int argc, char *argv[], const char *name)
{
	for (int index = 1; index < argc; ++index)
		if (!strcmp(argv[index], name))
			return 1;
	return 0;
}

static int init_headless_audio(void)
{
	static char sdl_audio_driver[] = "SDL_AUDIODRIVER=dummy";

	GameArg.SndNoMusic = 1;
	SDL_putenv(sdl_audio_driver);
	digi_select_system(GameArg.SndDisableSdlMixer ? SDLAUDIO_SYSTEM : SDLMIXER_SYSTEM);
	if (digi_init())
		return 0;
	digi_set_digi_volume(0);
	songs_set_volume(0);
	return 1;
}

static int init_headless_screen(char *error, size_t error_size)
{
	const int screen_w = (int) SM_W(Game_screen_mode);
	const int screen_h = (int) SM_H(Game_screen_mode);

	if (grd_curscreen)
		return 1;
	grd_curscreen = (grs_screen *) d_calloc(1, sizeof(grs_screen));
	MALLOC(headless_screen_pixels, unsigned char, screen_w *screen_h);
	if (!grd_curscreen || !headless_screen_pixels) {
		snprintf(error, error_size, "%s", "screen allocation failed");
		return 0;
	}
	memset(headless_screen_pixels, 0, (size_t) (screen_w * screen_h));
	grd_curscreen->sc_mode = Game_screen_mode;
	grd_curscreen->sc_w = (short) screen_w;
	grd_curscreen->sc_h = (short) screen_h;
	grd_curscreen->sc_aspect = fixdiv(grd_curscreen->sc_w * GameCfg.AspectX,
	                                  grd_curscreen->sc_h * GameCfg.AspectY);
	gr_init_canvas(&grd_curscreen->sc_canvas, headless_screen_pixels, BM_LINEAR, screen_w, screen_h);
	gr_set_current_canvas(NULL);
	return 1;
}

static int init_headless_metadata_runtime(int argc, char *argv[], char *error, size_t error_size)
{
	trace_dump_init("mem_init");
	mem_init();
	trace_dump_init("error_init");
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	trace_dump_init("physfs_init");
	PHYSFSX_init(argc, argv);
	trace_dump_init("con_init");
	con_init();
	if (GameArg.SysShowCmdHelp) {
		snprintf(error, error_size, "%s", "help requested");
		return 0;
	}
	trace_dump_init("archive_type_check");
	if (!PHYSFSX_checkSupportedArchiveTypes()) {
		snprintf(error, error_size, "%s", "archive type check failed");
		return 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	trace_dump_init("contfile_d2");
	if (!PHYSFSX_contfile_init("descent2.hog", 1) &&
	    !PHYSFSX_contfile_init("d2demo.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent2.hog or d2demo.hog; pass -hogdir <dir> with Descent 2 data files");
		return 0;
	}
#else
	trace_dump_init("contfile_d1");
	if (!PHYSFSX_contfile_init("descent.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent.hog; pass -hogdir <dir> with Descent data files");
		return 0;
	}
#endif
	trace_dump_init("load_text");
	load_text();
	trace_dump_init("read_config");
	ReadConfigFile();
	trace_dump_init("audio");
	if (!init_headless_audio()) {
		snprintf(error, error_size, "%s", "audio init failed");
		return 0;
	}
	trace_dump_init("archive_content");
	PHYSFSX_addArchiveContent();
	trace_dump_init("gamedata");
	gamedata_init();
	trace_dump_init("texmerge");
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		trace_dump_init("piggy_init_pigfile");
		piggy_init_pigfile(groupa_pig);
	}
#endif
	trace_dump_init("screen");
	if (!init_headless_screen(error, error_size))
		return 0;
	Screen_mode = SCREEN_GAME;
	trace_dump_init("init_game");
	init_game();
	Players[Player_num].callsign[0] = '\0';
	GameArg.SysUseNiceFPS = 0;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SysInputDemoNoRender = 1;
#endif
	trace_dump_init("runtime_done");
	return 1;
}

static int load_base_mission(const char *requested_mission, char *error, size_t error_size)
{
	if (requested_mission && *requested_mission) {
		std::vector<char> mission_name(requested_mission, requested_mission + strlen(requested_mission) + 1);

		trace_dump_init("load_mission_requested");
		if (load_mission_by_name(mission_name.data()))
			return 1;
		snprintf(error, error_size, "could not load requested mission %s", requested_mission);
		return 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	char d2_mission[] = "d2";
	char d2_demo_mission[] = "d2demo";

	trace_dump_init("load_mission_d2");
	if (load_mission_by_name(d2_mission))
		return 1;
	if (load_mission_by_name(d2_demo_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load d2 or d2demo mission");
	return 0;
#else
	char d1_mission[] = "";

	trace_dump_init("load_mission_d1");
	if (load_mission_by_name(d1_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load built-in d1 mission");
	return 0;
#endif
}

static int mount_extra_dir(const char *extra_dir, char *error, size_t error_size)
{
	if (!extra_dir || !*extra_dir)
		return 1;
	trace_dump_init("mount_extra_dir");
	if (PHYSFS_addToSearchPath(extra_dir, 0))
		return 1;
	snprintf(error, error_size, "could not mount extra dir %s", extra_dir);
	return 0;
}

static int dump_wall_clip_flags(int wall_num)
{
	int clip_num;

	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	clip_num = Walls[wall_num].clip_num;
	if (clip_num < 0 || clip_num >= Num_wall_anims)
		return 0;
	return WallAnims[clip_num].flags;
}

static void trace_topological_path(int level_num, const char *kind, int target_seg)
{
	static int parent[MAX_SEGMENTS];
	static int parent_side[MAX_SEGMENTS];
	static int queue[MAX_SEGMENTS];
	int start_seg = -1;
	int head = 0;
	int tail = 0;
	int seg;

	for (seg = 0; seg <= Highest_object_index; ++seg)
		if (Objects[seg].type == OBJ_PLAYER) {
			start_seg = Objects[seg].segnum;
			break;
		}
	if (start_seg < 0 || start_seg >= Num_segments || target_seg < 0 || target_seg >= Num_segments)
		return;
	for (seg = 0; seg < Num_segments; ++seg) {
		parent[seg] = -2;
		parent_side[seg] = -1;
	}
	parent[start_seg] = -1;
	queue[tail++] = start_seg;
	while (head < tail && parent[target_seg] == -2) {
		int current = queue[head++];
		int side;
		for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
			int child = Segments[current].children[side];
			if (child < 0 || child >= Num_segments || parent[child] != -2)
				continue;
			parent[child] = current;
			parent_side[child] = side;
			queue[tail++] = child;
		}
	}
	fprintf(stderr, "SECRET-AREA-DUMP TOPOLOGY level=%d kind=%s start=%d target=%d connected=%d\n",
	        level_num, kind, start_seg, target_seg, parent[target_seg] != -2);
	if (parent[target_seg] == -2)
		return;
	for (seg = target_seg; parent[seg] >= 0; seg = parent[seg]) {
		int from = parent[seg];
		int side = parent_side[seg];
		int reverse_side = find_connect_side(&Segments[from], &Segments[seg]);
		int wall = Segments[from].sides[side].wall_num;
		int reverse_wall = reverse_side >= 0 ? Segments[seg].sides[reverse_side].wall_num : -1;
		fprintf(stderr,
		        "SECRET-AREA-DUMP TOPOLOGY-EDGE level=%d kind=%s from=%d side=%d to=%d reverse_side=%d wall=%d wall_type=%d keys=%d flags=%d trigger=%d tmap2=%d reverse_wall=%d reverse_type=%d reverse_keys=%d reverse_flags=%d reverse_trigger=%d reverse_tmap2=%d\n",
		        level_num, kind, from, side, seg, reverse_side, wall,
		        wall >= 0 && wall < Num_walls ? Walls[wall].type : -1,
		        wall >= 0 && wall < Num_walls ? Walls[wall].keys : -1,
		        wall >= 0 && wall < Num_walls ? Walls[wall].flags : 0,
		        wall >= 0 && wall < Num_walls ? Walls[wall].trigger : -1,
		        Segments[from].sides[side].tmap_num2,
		        reverse_wall,
		        reverse_wall >= 0 && reverse_wall < Num_walls ? Walls[reverse_wall].type : -1,
		        reverse_wall >= 0 && reverse_wall < Num_walls ? Walls[reverse_wall].keys : -1,
		        reverse_wall >= 0 && reverse_wall < Num_walls ? Walls[reverse_wall].flags : 0,
		        reverse_wall >= 0 && reverse_wall < Num_walls ? Walls[reverse_wall].trigger : -1,
		        reverse_side >= 0 ? Segments[seg].sides[reverse_side].tmap_num2 : 0);
	}
}

static void trace_wall_inventory(int level_num, const char *level_file)
{
	int wall_type_counts[8] = { 0 };
	int hidden_clip_by_type[8] = { 0 };
#ifdef DXX_BUILD_DESCENT_II
	int opener_source_type_counts[8] = { 0 };
	int opener_target_type_counts[8] = { 0 };
	int opener_links = 0;
	int trigger_num;
#endif
	int type;
	int wall_num;
	int objnum;

	if (!getenv("DXX_SECRET_AREA_DUMP_TRACE"))
		return;
	for (wall_num = 0; wall_num < Num_walls; ++wall_num) {
		type = Walls[wall_num].type;
		if (type < 0 || type >= (int) (sizeof(wall_type_counts) / sizeof(wall_type_counts[0])))
			type = 0;
		wall_type_counts[type]++;
		if (dump_wall_clip_flags(wall_num) & WCF_HIDDEN)
			hidden_clip_by_type[type]++;
	}
	for (objnum = 0; objnum <= Highest_object_index; ++objnum) {
		const object *obj = &Objects[objnum];
		int key_id = -1;
		if (obj->type == OBJ_CNTRLCEN)
			fprintf(stderr, "SECRET-AREA-DUMP TARGET level=%d kind=reactor object=%d seg=%d\n",
			        level_num, objnum, obj->segnum);
#ifdef DXX_BUILD_DESCENT_II
		if (obj->type == OBJ_ROBOT && obj->id >= 0 && obj->id < N_robot_types && Robot_info[obj->id].boss_flag)
			fprintf(stderr, "SECRET-AREA-DUMP TARGET level=%d kind=boss object=%d id=%d seg=%d\n",
			        level_num, objnum, obj->id, obj->segnum);
#endif
		if (obj->type == OBJ_CNTRLCEN)
			trace_topological_path(level_num, "reactor", obj->segnum);
#ifdef DXX_BUILD_DESCENT_II
		if (obj->type == OBJ_ROBOT && obj->id >= 0 && obj->id < N_robot_types && Robot_info[obj->id].boss_flag)
			trace_topological_path(level_num, "boss", obj->segnum);
#endif
		if (obj->type == OBJ_POWERUP &&
		    (obj->id == POW_KEY_BLUE || obj->id == POW_KEY_RED || obj->id == POW_KEY_GOLD)) {
			key_id = obj->id;
			fprintf(stderr,
			        "SECRET-AREA-DUMP KEY level=%d object=%d id=%d seg=%d direct=1\n",
			        level_num, objnum, obj->id, obj->segnum);
		}
		if (obj->contains_type == OBJ_POWERUP && obj->contains_count > 0 &&
		    (obj->contains_id == POW_KEY_BLUE || obj->contains_id == POW_KEY_RED || obj->contains_id == POW_KEY_GOLD)) {
			key_id = obj->contains_id;
			fprintf(stderr,
			        "SECRET-AREA-DUMP KEY level=%d object=%d id=%d seg=%d direct=0 carrier_type=%d count=%d\n",
			        level_num, objnum, obj->contains_id, obj->segnum, obj->type, obj->contains_count);
		}
		if (key_id >= 0 && obj->segnum >= 0 && obj->segnum < Num_segments) {
			int side;
			trace_topological_path(level_num, "key", obj->segnum);
			for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
				int wall = Segments[obj->segnum].sides[side].wall_num;
				fprintf(stderr,
				        "SECRET-AREA-DUMP KEY-SIDE level=%d id=%d seg=%d side=%d child=%d wall=%d wall_type=%d keys=%d flags=%d trigger=%d\n",
				        level_num, key_id, obj->segnum, side, Segments[obj->segnum].children[side], wall,
				        wall >= 0 && wall < Num_walls ? Walls[wall].type : -1,
				        wall >= 0 && wall < Num_walls ? Walls[wall].keys : -1,
				        wall >= 0 && wall < Num_walls ? Walls[wall].flags : 0,
				        wall >= 0 && wall < Num_walls ? Walls[wall].trigger : -1);
			}
		}
	}
#ifdef DXX_BUILD_DESCENT_II
	for (trigger_num = 0; trigger_num < Num_triggers; ++trigger_num) {
		int link;
		int source_wall;

		if (Triggers[trigger_num].type != TT_OPEN_DOOR &&
		    Triggers[trigger_num].type != TT_ILLUSION_OFF &&
		    Triggers[trigger_num].type != TT_UNLOCK_DOOR &&
		    Triggers[trigger_num].type != TT_OPEN_WALL &&
		    Triggers[trigger_num].type != TT_ILLUSORY_WALL)
			continue;
		for (source_wall = 0; source_wall < Num_walls; ++source_wall) {
			int source_seg;
			int source_side;
			int source_child;
			int reverse_side = -1;
			int reverse_wall = -1;

			if (Walls[source_wall].trigger != trigger_num)
				continue;
			source_seg = Walls[source_wall].segnum;
			source_side = Walls[source_wall].sidenum;
			source_child = source_seg >= 0 && source_seg < Num_segments && source_side >= 0 && source_side < MAX_SIDES_PER_SEGMENT ? Segments[source_seg].children[source_side] : -1;
			if (source_child >= 0 && source_child < Num_segments) {
				reverse_side = find_connect_side(&Segments[source_seg], &Segments[source_child]);
				if (reverse_side >= 0)
					reverse_wall = Segments[source_child].sides[reverse_side].wall_num;
			}
			fprintf(stderr,
			        "SECRET-AREA-DUMP TRIGGER-SOURCE level=%d trigger=%d type=%d wall=%d wall_type=%d seg=%d side=%d child=%d reverse_side=%d reverse_wall=%d tmap2=%d\n",
			        level_num,
			        trigger_num,
			        Triggers[trigger_num].type,
			        source_wall,
			        Walls[source_wall].type,
			        source_seg,
			        source_side,
			        source_child,
			        reverse_side,
			        reverse_wall,
			        source_seg >= 0 && source_seg < Num_segments && source_side >= 0 && source_side < MAX_SIDES_PER_SEGMENT ? Segments[source_seg].sides[source_side].tmap_num2 : 0);
			type = Walls[source_wall].type;
			if (type < 0 || type >= (int) (sizeof(opener_source_type_counts) / sizeof(opener_source_type_counts[0])))
				type = 0;
			opener_source_type_counts[type]++;
		}
		for (link = 0; link < Triggers[trigger_num].num_links; ++link) {
			int seg = Triggers[trigger_num].seg[link];
			int side = Triggers[trigger_num].side[link];
			int target_wall;

			if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
				continue;
			target_wall = Segments[seg].sides[side].wall_num;
			fprintf(stderr,
			        "SECRET-AREA-DUMP TRIGGER-LINK level=%d trigger=%d type=%d link=%d seg=%d side=%d wall=%d\n",
			        level_num,
			        trigger_num,
			        Triggers[trigger_num].type,
			        link,
			        seg,
			        side,
			        target_wall);
			if (target_wall < 0 || target_wall >= Num_walls)
				continue;
			type = Walls[target_wall].type;
			if (type < 0 || type >= (int) (sizeof(opener_target_type_counts) / sizeof(opener_target_type_counts[0])))
				type = 0;
			opener_target_type_counts[type]++;
			++opener_links;
		}
	}
	for (trigger_num = 0; trigger_num < ControlCenterTriggers.num_links; ++trigger_num) {
		int seg = ControlCenterTriggers.seg[trigger_num];
		int side = ControlCenterTriggers.side[trigger_num];
		int wall = seg >= 0 && seg < Num_segments && side >= 0 && side < MAX_SIDES_PER_SEGMENT ? Segments[seg].sides[side].wall_num : -1;
		fprintf(stderr, "SECRET-AREA-DUMP CONTROL-CENTER-LINK level=%d link=%d seg=%d side=%d wall=%d\n",
		        level_num, trigger_num, seg, side, wall);
	}
#endif
	fprintf(stderr,
	        "SECRET-AREA-DUMP WALLS level=%d file=%s walls=%d normal=%d blastable=%d door=%d illusion=%d open=%d hidden_normal=%d hidden_blastable=%d hidden_door=%d hidden_illusion=%d hidden_open=%d\n",
	        level_num,
	        level_file ? level_file : "",
	        Num_walls,
	        wall_type_counts[WALL_NORMAL],
	        wall_type_counts[WALL_BLASTABLE],
	        wall_type_counts[WALL_DOOR],
	        wall_type_counts[WALL_ILLUSION],
	        wall_type_counts[WALL_OPEN],
	        hidden_clip_by_type[WALL_NORMAL],
	        hidden_clip_by_type[WALL_BLASTABLE],
	        hidden_clip_by_type[WALL_DOOR],
	        hidden_clip_by_type[WALL_ILLUSION],
	        hidden_clip_by_type[WALL_OPEN]);
#ifdef DXX_BUILD_DESCENT_II
	fprintf(stderr,
	        "SECRET-AREA-DUMP TRIGGERS level=%d opener_links=%d source_open=%d source_overlay=%d target_closed=%d target_door=%d target_illusion=%d target_open=%d\n",
	        level_num,
	        opener_links,
	        opener_source_type_counts[WALL_OPEN],
	        opener_source_type_counts[WALL_OVERLAY],
	        opener_target_type_counts[WALL_CLOSED],
	        opener_target_type_counts[WALL_DOOR],
	        opener_target_type_counts[WALL_ILLUSION],
	        opener_target_type_counts[WALL_OPEN]);
#endif
	fflush(stderr);
}

static nlohmann::ordered_json serialize_int_array(const int *values, int count)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();

	for (int index = 0; index < count; ++index)
		result.push_back(values[index]);
	return result;
}

static nlohmann::ordered_json serialize_vector(const vms_vector &value)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();

	result.push_back((int) value.x);
	result.push_back((int) value.y);
	result.push_back((int) value.z);
	return result;
}

static int count_loaded_coop_start_objects()
{
	int count = 0;
	int player_or_coop_index = 0;

	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *obj = &Objects[objnum];

		if (obj->type != OBJ_PLAYER && obj->type != OBJ_GHOST && obj->type != OBJ_COOP)
			continue;
		if (player_or_coop_index == 0 || obj->type == OBJ_COOP)
			++count;
		++player_or_coop_index;
	}
	return count;
}

struct CoopStartRange {
	int min = 0;
	int max = 0;

	void add(int value)
	{
		if (value <= 0)
			return;
		if (!min || value < min)
			min = value;
		if (value > max)
			max = value;
	}

	std::string text() const
	{
		char buffer[32];

		if (!min)
			return "";
		if (min == max)
			snprintf(buffer, sizeof(buffer), "%d", min);
		else
			snprintf(buffer, sizeof(buffer), "%d-%d", min, max);
		return buffer;
	}
};

static nlohmann::ordered_json serialize_coop_start_slot(int slot, int real_start_count)
{
	nlohmann::ordered_json result;

	result["slot"] = slot;
	result["generated"] = slot >= real_start_count;
	result["seg"] = Player_init[slot].segnum;
	result["pos"] = serialize_vector(Player_init[slot].pos);
	result["objnum"] = Players[slot].objnum;
	if (Players[slot].objnum >= 0 && Players[slot].objnum <= Highest_object_index)
		result["object_type"] = (int) Objects[Players[slot].objnum].type;
	else
		result["object_type"] = -1;
	return result;
}

static int positions_match(const vms_vector &a, const vms_vector &b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

static nlohmann::ordered_json serialize_coop_level(int level_num, const char *level_file)
{
	const int previous_game_mode = Game_mode;
	int real_start_count;
	int duplicate_pairs = 0;
	int too_close_pairs = 0;
	int minimum_pair_distance = 0x7fffffff;
	nlohmann::ordered_json result;
	nlohmann::ordered_json starts = nlohmann::ordered_json::array();

	if (load_level(level_file)) {
		fprintf(stderr, "COOP-START-DUMP FAIL level could not load %s\n", level_file ? level_file : "<null>");
		secret_area_dump_failed = 1;
		return result;
	}
	Current_level_num = level_num;
	real_start_count = count_loaded_coop_start_objects();
	Game_mode = GM_NETWORK | GM_MULTI_ROBOTS | GM_MULTI_COOP;
	gameseq_init_network_players();
	Game_mode = previous_game_mode;
	for (int slot = 0; slot < MAX_PLAYERS; ++slot) {
		for (int other = 0; other < slot; ++other) {
			int distance = vm_vec_dist_quick(&Player_init[slot].pos, &Player_init[other].pos);

			if (distance < minimum_pair_distance)
				minimum_pair_distance = distance;
			if (positions_match(Player_init[slot].pos, Player_init[other].pos))
				++duplicate_pairs;
			if (distance < Polygon_models[Player_ship->model_num].rad * 2)
				++too_close_pairs;
		}
		starts.push_back(serialize_coop_start_slot(slot, real_start_count));
	}
	result["level_num"] = level_num;
	result["level_name"] = Current_level_name;
	result["level_file"] = level_file ? level_file : "";
	result["real_start_count"] = real_start_count;
	result["num_net_player_positions"] = NumNetPlayerPositions;
	result["max_players"] = MAX_PLAYERS;
	result["ship_radius"] = Polygon_models[Player_ship->model_num].rad;
	result["minimum_allowed_distance"] = Polygon_models[Player_ship->model_num].rad * 2;
	result["minimum_pair_distance"] = minimum_pair_distance == 0x7fffffff ? 0 : minimum_pair_distance;
	result["duplicate_position_pairs"] = duplicate_pairs;
	result["too_close_pairs"] = too_close_pairs;
	result["starts"] = starts;
	return result;
}

static nlohmann::ordered_json build_coop_start_dump()
{
	nlohmann::ordered_json root;
	nlohmann::ordered_json levels = nlohmann::ordered_json::array();

	root["schema"] = "dxx-coop-start-fanout-v1";
#ifdef DXX_BUILD_DESCENT_II
	root["game"] = "d2";
#else
	root["game"] = "d1";
#endif
	root["mission_name"] = Current_mission_longname;
	root["mission_filename"] = Current_mission_filename;
	root["max_players"] = MAX_PLAYERS;
	for (int level = 1; level <= Last_level; ++level)
		levels.push_back(serialize_coop_level(level, Level_names[level - 1]));
	for (int level = -1; level >= Last_secret_level; --level)
		levels.push_back(serialize_coop_level(level, Secret_level_names[-level - 1]));
	root["levels"] = levels;
	return root;
}

static nlohmann::ordered_json serialize_entrance(const secret_area_entrance &entrance)
{
	nlohmann::ordered_json result;

	result["seg"] = entrance.seg;
	result["side"] = entrance.side;
	result["secret_seg"] = entrance.secret_seg;
	result["wall_num"] = entrance.wall_num;
	result["wall_type"] = entrance.wall_num >= 0 && entrance.wall_num < Num_walls ? (int) Walls[entrance.wall_num].type : -1;
	result["wall_flags"] = entrance.wall_num >= 0 && entrance.wall_num < Num_walls ? (int) Walls[entrance.wall_num].flags : 0;
	result["wall_clip_flags"] = dump_wall_clip_flags(entrance.wall_num);
	return result;
}

static nlohmann::ordered_json serialize_item(const secret_area_item &item)
{
	char fallback[32];
	nlohmann::ordered_json result;

	snprintf(fallback, sizeof(fallback), "powerup %d", item.id);
	result["id"] = item.id;
	result["name"] = item.name[0] ? item.name : fallback;
	result["count"] = item.count;
	result["direct_count"] = item.direct_count;
	result["contained_count"] = item.contained_count;
	return result;
}

static nlohmann::ordered_json serialize_secret(const secret_area_entry &secret)
{
	char label[16];
	nlohmann::ordered_json result;
	nlohmann::ordered_json items = nlohmann::ordered_json::array();
	nlohmann::ordered_json entrances = nlohmann::ordered_json::array();

	snprintf(label, sizeof(label), "S%d", secret.display_index);
	result["id"] = label;
	result["display_index"] = secret.display_index;
	result["entry_distance"] = secret.entry_distance;
	result["entry_seg"] = secret.entry_seg;
	result["entry_side"] = secret.entry_side;
	result["lowest_segment"] = secret.lowest_segment;
	result["label_pos"] = serialize_int_array(secret.label_pos, 3);
	result["robot_count"] = secret.robot_count;
	result["robotmaker_count"] = secret.robotmaker_count;
	result["item_count"] = secret.item_count;
	for (int index = 0; index < secret.item_count; ++index)
		items.push_back(serialize_item(secret.items[index]));
	result["items"] = items;
	for (int index = 0; index < secret.entrance_count; ++index)
		entrances.push_back(serialize_entrance(secret.entrances[index]));
	result["entrances"] = entrances;
	result["segments"] = serialize_int_array(secret.segments, secret.segment_count);
	return result;
}

static const char *headless_metadata_key_name(int key_index)
{
	switch (key_index) {
		case 0:
			return "blue";
		case 1:
			return "red";
		case 2:
			return "gold";
		default:
			return "";
	}
}

static nlohmann::ordered_json serialize_route_position(const int pos[3])
{
	return {
		{ "x", pos[0] / LEVEL_METADATA_FIX_SCALE },
		{ "y", pos[1] / LEVEL_METADATA_FIX_SCALE },
		{ "z", pos[2] / LEVEL_METADATA_FIX_SCALE }
	};
}

static nlohmann::ordered_json serialize_route_steps(const level_metadata_state *metadata)
{
	nlohmann::ordered_json steps = nlohmann::ordered_json::array();
	int count;

	if (!metadata)
		return steps;
	count = metadata->route_step_count;
	if (count < 0)
		count = 0;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	for (int index = 0; index < count; ++index) {
		const level_metadata_route_step &step = metadata->route_steps[index];
		nlohmann::ordered_json item;
		item["index"] = index;
		item["kind"] = level_metadata_route_step_kind_name(step.kind);
		item["activation_kind"] = level_metadata_route_activation_kind_name(step.activation_kind);
		if (step.label[0])
			item["label"] = step.label;
		if (step.seg >= 0)
			item["seg"] = step.seg;
		if (step.side >= 0)
			item["side"] = step.side;
		if (step.wall_num >= 0)
			item["wall"] = step.wall_num;
		if (step.label_pos_valid)
			item["label_pos"] = serialize_route_position(step.label_pos);
		if (step.distance_from_previous > 0.0)
			item["distance"] = step.distance_from_previous;
		if (step.kind == LEVEL_METADATA_ROUTE_KEY && step.key_index >= 0)
			item["key"] = headless_metadata_key_name(step.key_index);
		if (step.trigger_num >= 0)
			item["trigger"] = step.trigger_num;
		if (step.trigger_type >= 0)
			item["trigger_type_id"] = step.trigger_type;
		if (step.trigger_type_name[0])
			item["trigger_type"] = step.trigger_type_name;
		if (step.opened_link_count > 0) {
			nlohmann::ordered_json opened = nlohmann::ordered_json::array();
			for (int link = 0; link < step.opened_link_count; ++link) {
				nlohmann::ordered_json open;
				if (step.opened_link_seg[link] >= 0)
					open["seg"] = step.opened_link_seg[link];
				if (step.opened_link_side[link] >= 0)
					open["side"] = step.opened_link_side[link];
				if (step.opened_link_wall[link] >= 0)
					open["wall"] = step.opened_link_wall[link];
				opened.push_back(open);
			}
			item["opens"] = opened;
		}
		steps.push_back(item);
	}
	return steps;
}

static nlohmann::ordered_json serialize_metadata_notes(const level_metadata_state *metadata)
{
	nlohmann::ordered_json notes = nlohmann::ordered_json::array();

	if (!metadata)
		return notes;
	if (metadata->route_note[0])
		notes.push_back(metadata->route_note);
	if (metadata->guidebot_placement_note[0] &&
	    strcmp(metadata->guidebot_placement_note, metadata->route_note))
		notes.push_back(metadata->guidebot_placement_note);
	if (metadata->guidebot_note[0] && strcmp(metadata->guidebot_note, metadata->route_note))
		notes.push_back(metadata->guidebot_note);
	return notes;
}

static void count_level_objects(int *robots, int *hostages)
{
	*robots = 0;
	*hostages = 0;
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		const object *obj = &Objects[objnum];
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->type == OBJ_ROBOT)
			++*robots;
		else if (obj->type == OBJ_HOSTAGE)
			++*hostages;
	}
}

static nlohmann::ordered_json serialize_current_level(int level_num, const char *level_file)
{
	const level_metadata_state *metadata = level_metadata_get_canonical_state();
	const secret_area_state *state = secret_area_get_state();
	int total = secret_area_total(state);
	int robots = 0;
	int hostages = 0;
	nlohmann::ordered_json result;
	nlohmann::ordered_json secrets = nlohmann::ordered_json::array();

	count_level_objects(&robots, &hostages);
	result["level_num"] = level_num;
	result["secret"] = level_num < 0;
	result["level_name"] = Current_level_name;
	result["level_file"] = level_file ? level_file : "";
	result["robot_count"] = robots;
	result["hostage_count"] = hostages;
	result["scanner_enabled"] = state->enabled ? true : false;
	result["disabled_reason"] = secret_area_disabled_reason_name(state->disabled_reason);
	result["raw_candidate_count"] = state->raw_candidate_count;
	result["final_candidate_count"] = state->final_candidate_count;
	result["secret_count"] = total;
	result["energy_center_count"] = metadata ? metadata->energy_center_count : 0;
	result["energy_center_raw_count"] = metadata ? metadata->energy_center_raw_count : 0;
	result["energy_center_segment_count"] = metadata ? metadata->energy_center_segment_count : 0;
	result["energy_center_group_distance"] = metadata ? metadata->energy_center_group_distance : 0;
	result["energy_center_nearest_raw_distance"] = metadata ? metadata->energy_center_nearest_raw_distance : 0;
	result["matcen_count"] = metadata ? metadata->matcen_count : 0;
	result["matcen_raw_count"] = metadata ? metadata->matcen_raw_count : 0;
	result["matcen_segment_count"] = metadata ? metadata->matcen_segment_count : 0;
	result["mine_volume"] = metadata ? metadata->mine_volume : 0.0;
	result["mine_volume_normalized"] = metadata ? metadata->mine_volume_normalized : 0.0;
	result["travel_distance"] = metadata ? metadata->travel_distance : 0.0;
	result["travel_time_seconds"] = metadata ? metadata->travel_time_seconds : 0;
	result["notes"] = serialize_metadata_notes(metadata);
	result["guidebot_count"] = metadata ? metadata->guidebot_count : 0;
	result["guidebot_placed"] = metadata && metadata->guidebot_placed != 0;
	result["guidebot_accessible"] = metadata && metadata->guidebot_accessible != 0;
	result["guidebot_placement_note"] = metadata && metadata->guidebot_placement_note[0] ? metadata->guidebot_placement_note : "";
	result["guidebot_note"] = metadata && metadata->guidebot_note[0] ? metadata->guidebot_note : "";
	result["route_status"] = metadata ? level_metadata_route_status_name(metadata->route_status) : "failed";
	result["route_problem"] = metadata && metadata->route_problem[0] ? metadata->route_problem : "";
	result["route_note"] = metadata && metadata->route_note[0] ? metadata->route_note : "";
	result["route_steps"] = serialize_route_steps(metadata);
	for (int index = 0; index < total; ++index)
		secrets.push_back(serialize_secret(state->secrets[index]));
	result["secrets"] = secrets;
	return result;
}

static nlohmann::ordered_json serialize_failed_level(int level_num, const char *level_file, const char *problem)
{
	nlohmann::ordered_json result;

	result["level_num"] = level_num;
	result["secret"] = level_num < 0;
	result["level_name"] = "";
	result["level_file"] = level_file ? level_file : "";
	result["robot_count"] = 0;
	result["hostage_count"] = 0;
	result["scanner_enabled"] = false;
	result["disabled_reason"] = secret_area_disabled_reason_name(SECRET_AREA_DISABLED_INVALID_VIEW);
	result["raw_candidate_count"] = 0;
	result["final_candidate_count"] = 0;
	result["secret_count"] = 0;
	result["energy_center_count"] = 0;
	result["energy_center_raw_count"] = 0;
	result["energy_center_segment_count"] = 0;
	result["energy_center_group_distance"] = 0;
	result["energy_center_nearest_raw_distance"] = 0;
	result["matcen_count"] = 0;
	result["matcen_raw_count"] = 0;
	result["matcen_segment_count"] = 0;
	result["mine_volume"] = 0.0;
	result["mine_volume_normalized"] = 0.0;
	result["travel_distance"] = 0.0;
	result["travel_time_seconds"] = 0;
	result["notes"] = nlohmann::ordered_json::array();
	result["guidebot_count"] = 0;
	result["guidebot_placed"] = false;
	result["guidebot_accessible"] = false;
	result["guidebot_placement_note"] = "";
	result["guidebot_note"] = "";
	result["route_status"] = "failed";
	result["route_problem"] = problem ? problem : "could not load level";
	result["route_note"] = "";
	result["route_steps"] = nlohmann::ordered_json::array();
	result["secrets"] = nlohmann::ordered_json::array();
	result["status"] = "failed";
	result["problems"] = nlohmann::ordered_json::array({ problem ? problem : "could not load level" });
	return result;
}

static int dump_level(nlohmann::ordered_json &levels, int level_num, const char *level_file,
                      CoopStartRange *coop_start_range)
{
	const secret_area_state *state;
	int total;

	trace_dump_init("load_level");
	if (!level_file || !level_file[0] || !PHYSFSX_exists(level_file, 1)) {
		fprintf(stderr, "SECRET-AREA-DUMP WARN level missing %s\n", level_file ? level_file : "<null>");
		levels.push_back(serialize_failed_level(level_num, level_file, "level file is missing"));
		if (level_num < 0)
			++secret_area_missing_secret_levels;
		else
			secret_area_dump_failed = 1;
		return 0;
	}
	if (load_level(level_file)) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL level could not load %s\n", level_file ? level_file : "<null>");
		levels.push_back(serialize_failed_level(level_num, level_file, "could not load level"));
		secret_area_dump_failed = 1;
		return 0;
	}
	trace_wall_inventory(level_num, level_file);
	Current_level_num = level_num;
	if (coop_start_range)
		coop_start_range->add(count_loaded_coop_start_objects());
	trace_dump_init("rescan_level");
	secret_area_rescan_current_level();
	if (route_shadow_strict) {
		route_edge_shadow_summary shadow = {};
		route_planner_shadow_summary planner_shadow = {};
		if (!level_metadata_get_route_edge_shadow(&shadow)) {
			fprintf(stderr, "SECRET-AREA-DUMP FAIL route edge shadow unavailable level=%d file=%s\n",
			        level_num, level_file ? level_file : "");
			secret_area_dump_failed = 1;
		} else if (shadow.mismatch_count) {
			fprintf(stderr,
			        "SECRET-AREA-DUMP FAIL route edge shadow mismatch level=%d file=%s compared=%d mismatches=%d first=%d:%d legacy=%d shared=%d\n",
			        level_num, level_file ? level_file : "", shadow.compared_edge_count,
			        shadow.mismatch_count, shadow.first_mismatch_segment, shadow.first_mismatch_side,
			        shadow.first_legacy_cost, shadow.first_shared_cost);
			secret_area_dump_failed = 1;
		}
		if (!level_metadata_get_route_planner_shadow(&planner_shadow)) {
			fprintf(stderr, "SECRET-AREA-DUMP FAIL route planner shadow unavailable level=%d file=%s\n",
			        level_num, level_file ? level_file : "");
			secret_area_dump_failed = 1;
		} else if (planner_shadow.mismatch_count || planner_shadow.target_mismatch_count) {
			fprintf(stderr,
			        "SECRET-AREA-DUMP FAIL route planner shadow mismatch level=%d file=%s states=%d compared=%d mismatches=%d first_state=%d first_mode=%s first_segment=%d legacy_reachable=%d shared_reachable=%d legacy_progress=%d shared_progress=%d legacy_parent=%d:%d shared_parent=%d:%d legacy_distance=%.17g shared_distance=%.17g compared_targets=%d target_mismatches=%d first_target=%d:%d legacy_target_count=%d shared_target_count=%d legacy_target_seg=%d shared_target_seg=%d\n",
			        level_num, level_file ? level_file : "", planner_shadow.compared_progress_state_count,
			        planner_shadow.compared_node_count,
			        planner_shadow.mismatch_count, planner_shadow.first_mismatch_progress_state,
			        planner_shadow.first_mismatch_optimistic ? "optimistic" : "pessimistic",
			        planner_shadow.first_mismatch_segment,
			        planner_shadow.first_legacy_reachable, planner_shadow.first_shared_reachable,
			        planner_shadow.first_legacy_progress_weight, planner_shadow.first_shared_progress_weight,
			        planner_shadow.first_legacy_parent_segment, planner_shadow.first_legacy_parent_side,
			        planner_shadow.first_shared_parent_segment, planner_shadow.first_shared_parent_side,
			        planner_shadow.first_legacy_distance, planner_shadow.first_shared_distance,
			        planner_shadow.compared_target_count, planner_shadow.target_mismatch_count,
			        planner_shadow.first_target_category, planner_shadow.first_target_index,
			        planner_shadow.first_legacy_target_count, planner_shadow.first_shared_target_count,
			        planner_shadow.first_legacy_target_segment, planner_shadow.first_shared_target_segment);
			secret_area_dump_failed = 1;
		}
	}
	state = secret_area_get_state();
	total = secret_area_total(state);
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
		fprintf(stderr, "SECRET-AREA-DUMP TRACE level=%d file=%s enabled=%d reason=%s raw=%d final=%d total=%d\n",
		        level_num,
		        level_file ? level_file : "",
		        state->enabled,
		        secret_area_disabled_reason_name(state->disabled_reason),
		        state->raw_candidate_count,
		        state->final_candidate_count,
		        total);
		fflush(stderr);
	}
	levels.push_back(serialize_current_level(level_num, level_file));
	return total;
}

static nlohmann::ordered_json build_dump(int *total_secrets)
{
	nlohmann::ordered_json root;
	nlohmann::ordered_json levels = nlohmann::ordered_json::array();
	int secret_total = 0;
	CoopStartRange coop_start_range;

	root["schema"] = "dxx-secret-area-baseline-v1";
	root["algorithm_version"] = 2;
	root["max_generated_secrets"] = SECRET_AREA_MAX_GENERATED;
#ifdef DXX_BUILD_DESCENT_II
	root["game"] = "d2";
#else
	root["game"] = "d1";
#endif
	root["mission_name"] = Current_mission_longname;
	root["mission_filename"] = Current_mission_filename;
	for (int level = 1; level <= Last_level; ++level)
		secret_total += dump_level(levels, level, Level_names[level - 1], &coop_start_range);
	for (int level = -1; level >= Last_secret_level; --level)
		secret_total += dump_level(levels, level, Secret_level_names[-level - 1], &coop_start_range);
	if (!coop_start_range.text().empty())
		root["coop_starts"] = coop_start_range.text();
	root["levels"] = levels;
	root["problems"] = nlohmann::ordered_json::array();
	if (secret_area_missing_secret_levels)
		root["problems"].push_back("one or more secret levels are referenced but missing");
	trace_dump_init("assemble_total");
	root["total_secret_count"] = secret_total;
	trace_dump_init("write_dump_done");
	if (total_secrets)
		*total_secrets = secret_total;
	return root;
}

int main(int argc, char *argv[])
{
	char error[256] = "";
	const char *json_out = find_arg_value(argc, argv, "-secretarea-json-out");
	const char *coop_starts_json_out = find_arg_value(argc, argv, "-coop-starts-json-out");
	const char *mission = find_arg_value(argc, argv, "-mission");
	const char *extra_dir = find_arg_value(argc, argv, "-extra-dir");
	int total_secrets = 0;
	route_shadow_strict = has_arg(argc, argv, "-route-shadow-strict");

	if (!json_out && !coop_starts_json_out) {
		fprintf(stderr, "usage: %s (-secretarea-json-out <path> | -coop-starts-json-out <path>) [-hogdir <game-data-dir>] [-extra-dir <mission-dir>] [-mission <mission-name>] [-route-shadow-strict]\n",
		        argc > 0 ? argv[0] : "dxx-redux-headless-metadata");
		return 1;
	}
	if (!init_headless_metadata_runtime(argc, argv, error, sizeof(error))) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL init %s\n", error[0] ? error : "runtime init failed");
		return 1;
	}
	if (!mount_extra_dir(extra_dir, error, sizeof(error))) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL extra-dir %s\n", error[0] ? error : "extra dir mount failed");
		return 1;
	}
	if (!load_base_mission(mission, error, sizeof(error))) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL mission %s\n", error[0] ? error : "mission load failed");
		return 1;
	}

	if (coop_starts_json_out) {
		trace_dump_init("open_coop_starts_output");
		std::ofstream stream(coop_starts_json_out);
		if (!stream) {
			fprintf(stderr, "COOP-START-DUMP FAIL output could not open %s\n", coop_starts_json_out);
			return 1;
		}
		stream << dump_metadata_json(build_coop_start_dump()) << "\n";
		if (secret_area_dump_failed)
			return 1;
		if (!stream) {
			fprintf(stderr, "COOP-START-DUMP FAIL output could not write %s\n", coop_starts_json_out);
			return 1;
		}
		printf("COOP-START-DUMP OK out=%s\n", coop_starts_json_out);
		return 0;
	}

	trace_dump_init("open_output");
	std::ofstream stream(json_out);
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not open %s\n", json_out);
		return 1;
	}
	stream << dump_metadata_json(build_dump(&total_secrets)) << "\n";
	if (secret_area_dump_failed)
		return 1;
	trace_dump_init("write_done");
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not write %s\n", json_out);
		return 1;
	}
	printf("SECRET-AREA-DUMP OK secrets=%d out=%s\n", total_secrets, json_out);
	return 0;
}
