#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "args.h"
#include "bm.h"
#include "console.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "gauges.h"
#include "iff.h"
#include "mission.h"
#include "multi.h"
#include "newdemo.h"
#include "object.h"
#include "palette.h"
#include "physfsx.h"
#include "piggy.h"
#include "polyobj.h"
#include "powerup.h"
#include "robot.h"
#include "text.h"
#include "u_mem.h"
#ifdef DXX_BUILD_DESCENT_II
extern int Robot_replacements_loaded;
extern int Gamesave_num_players;
int load_mission_ham(void);
#else
void load_hxm(char *filename);
#endif
}

#pragma push_macro("PATH_MAX")
#undef PATH_MAX
#ifdef DXX_BUILD_DESCENT_II
#include "../../d2/xmodel/xcfile.h"
#else
#include "../../d1/xmodel/xcfile.h"
#endif
#pragma pop_macro("PATH_MAX")

static void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

using bytes = std::vector<unsigned char>;

static void set_int(bytes &data, size_t offset, int value)
{
	for (unsigned i = 0; i < 4; ++i)
		data[offset + i] = static_cast<unsigned char>(static_cast<unsigned>(value) >> (i * 8));
}

static void append_int(bytes &data, int value)
{
	const size_t offset = data.size();
	data.resize(offset + 4);
	set_int(data, offset, value);
}

static void append(bytes &data, const bytes &record)
{
	data.insert(data.end(), record.begin(), record.end());
}

static bytes model_record(fix radius)
{
	// Disk polymodels have a 32-bit pointer placeholder regardless of host ABI
	bytes record(734);
	set_int(record, 0, 1);
	set_int(record, 4, 2); // a two-byte OP_EOF model
	set_int(record, 726, radius);
	return record;
}

static void write_fixture(const char *name, const bytes &data)
{
	PHYSFS_file *file = PHYSFS_openWrite(name);
	require(file != nullptr, "create fixture");
	require(PHYSFS_writeBytes(file, data.data(), data.size()) == static_cast<PHYSFS_sint64>(data.size()), "write fixture");
	require(PHYSFS_close(file) != 0, "close fixture");
}

static bytes hxm_header()
{
	bytes data;
	append_int(data, 0x21584d48);
	append_int(data, 1);
	return data;
}

static void test_seek()
{
	write_fixture("seek.dat", { 1, 2, 3, 4 });
	CFile file;
	require(file.Open("seek.dat", "", "rb", 0) != 0, "open seek fixture");
	require(file.Seek(2, SEEK_SET) == 0, "seek success returns zero at nonzero offset");
	require(file.Seek(-1, SEEK_CUR) == 0, "relative seek");
	require(file.Seek(-1, SEEK_END) == 0, "end-relative seek");
	require(file.Seek(0, -99) == -1, "invalid seek method returns error");
}

static void test_lives()
{
	Player_num = 0;
	Game_mode = 0;
	Newdemo_state = ND_STATE_NORMAL;
	cheats.enabled = 0;
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	Players[0].shields = F1_0;
	Powerup_info[POW_EXTRA_LIFE].hit_sound = -1;
	Text_string[10] = const_cast<char *>("Extra life");
	object pickup = {};
	pickup.type = OBJ_POWERUP;
	pickup.id = POW_EXTRA_LIFE;
	Players[0].lives = 254;
	require(do_powerup(&pickup) != 0 && Players[0].lives == 255, "254 lives can consume one life");
	require(do_powerup(&pickup) == 0 && Players[0].lives == 255, "255 lives cannot wrap on pickup");
	Players[0].score = 49999;
	add_points_to_score(1);
	require(Players[0].lives == 255, "normal scoring cannot wrap lives");
	Players[0].lives = 254;
	Players[0].score = 49999;
	add_bonus_points_to_score(100001);
	require(Players[0].lives == 255, "multiple bonus lives saturate");
	add_bonus_points_to_score(50000);
	require(Players[0].lives == 255, "bonus scoring at cap");
}

#ifdef DXX_BUILD_DESCENT_II
static bytes robot_record(int model, fix mass, fix drag)
{
	bytes record(480);
	set_int(record, 0, model);
	set_int(record, 136, mass);
	set_int(record, 140, drag);
	set_int(record, 476, 0xabcd);
	return record;
}

static void make_robot_fixtures()
{
	bytes ham;
	append_int(ham, 0x214d4148); // HAM!
	append_int(ham, 3);
	for (int i = 0; i < 5; ++i) append_int(ham, 0); // textures, sounds, clips, effects, walls
	append_int(ham, 66);
	for (int i = 0; i < 66; ++i) append(ham, robot_record(0, F1_0, F1_0 / 16));
	for (int i = 0; i < 3; ++i) append_int(ham, 0); // joints, weapons, powerups
	append_int(ham, 166);
	for (int i = 0; i < 166; ++i) append(ham, model_record(F1_0));
	append(ham, bytes(166 * 2));
	for (int i = 0; i < 166 * 2; ++i) append_int(ham, -1);
	append_int(ham, 0);   // gauges
	append_int(ham, 422); // base object bitmaps and their pointers
	append(ham, bytes(422 * 4));
	append(ham, bytes(132));                        // player ship
	for (int i = 0; i < 4; ++i) append_int(ham, 0); // cockpits, multiplayer bitmap, reactors, marker
	append(ham, bytes(2));                          // bitmap translation
	write_fixture("descent2.ham", ham);

	bytes vham;
	append_int(vham, 0x5848414d); // MAKE_SIG('X', 'H', 'A', 'M') stored little-endian
	append_int(vham, 1);
	append_int(vham, 0); // weapons
	append_int(vham, 1);
	append(vham, robot_record(166, 2 * F1_0, F1_0 / 8));
	append_int(vham, 0); // joints
	append_int(vham, 1);
	append(vham, model_record(2 * F1_0));
	append(vham, bytes(2));
	append_int(vham, -1);
	append_int(vham, -1);
	append_int(vham, 0); // bitmaps
	append_int(vham, 0); // bitmap pointers
	write_fixture("testv.ham", vham);

	bytes hxm = hxm_header();
	append_int(hxm, 1);
	append_int(hxm, 66);
	append(hxm, robot_record(166, 7 * F1_0, F1_0 / 4));
	append_int(hxm, 0); // joints
	append_int(hxm, 1);
	append_int(hxm, 166);
	append(hxm, model_record(4 * F1_0));
	append(hxm, bytes(2));
	append_int(hxm, -1);
	append_int(hxm, -1);
	append_int(hxm, 0);
	append_int(hxm, 0);
	write_fixture("custom.HXM", hxm);

	bytes exit;
	append(exit, model_record(80 * F1_0));
	append(exit, model_record(90 * F1_0));
	append(exit, bytes(4));
	write_fixture("exit.ham", exit);
	unsigned char pixels[64 * 64] = {};
	unsigned char palette[768] = {};
	grs_bitmap bitmap = {};
	gr_init_bitmap(&bitmap, BM_LINEAR, 0, 0, 64, 64, 64, pixels);
	for (const char *name : { "steel1.bbm", "rbot061.bbm", "rbot062.bbm", "rbot063.bbm" })
		require(iff_write_bitmap(const_cast<char *>(name), &bitmap, palette) == IFF_NO_ERROR, "write exit bitmap");
}

static void test_robot_reload()
{
	make_robot_fixtures();
	Mission mission = {};
	mission.filename = const_cast<char *>("testv");
	mission.enhanced = 2;
	mission.descent_version = 2;
	Current_mission = &mission;
	Num_bitmap_files = 1;
	require(load_mission_ham() != 0, "load synthetic mission HAM/V-HAM");
	require(N_polygon_models == 167 && N_robot_types == 67, "extra mission tables loaded");
	std::memset(Objects, 0, sizeof(object) * 3);
	Highest_object_index = 2;
	Objects[0].type = OBJ_PLAYER;
	Objects[0].id = 3;
	Objects[1].type = OBJ_POWERUP;
	Objects[1].id = POW_ENERGY;
	Objects[2].type = OBJ_ROBOT;
	Objects[2].id = 66;
	Objects[2].render_type = RT_POLYOBJ;
	Objects[2].movement_type = MT_PHYSICS;
	Objects[2].lifeleft = 123;
	Objects[2].shields = 45 * F1_0;
	Gamesave_num_org_robots = 1;
	Gamesave_num_players = 4;
	Game_mode = GM_MULTI | GM_NETWORK | GM_MULTI_COOP;
	PowerupsInMine[POW_ENERGY] = 5;
	MaxPowerupsAllowed[POW_ENERGY] = 8;
	const object player_before = Objects[0], powerup_before = Objects[1];

	for (int pass = 0; pass < 3; ++pass) {
		load_level_robots_file("plain.rl2");
		require(Objects[2].size == 2 * F1_0 && Objects[2].rtype.pobj_info.model_num == 166, "base mission robot properties restored");
		require(Objects[2].mtype.phys_info.mass == 2 * F1_0, "base robot mass restored");
		load_level_robots_file("custom.rl2");
		require(Objects[2].size == 4 * F1_0 && Objects[2].mtype.phys_info.mass == 7 * F1_0 && Objects[2].mtype.phys_info.drag == F1_0 / 4, "HXM changes applied to existing objects");
		load_level_robots_file("plain.rl2");
		require(Objects[2].size == 2 * F1_0, "next level removes previous HXM");
		require(load_exit_models() == 1, "load actual exit assets");
		require(Polygon_models[166].rad == 80 * F1_0 && Robot_replacements_loaded, "exit replaced robot model and marked tables dirty");
		reset_level_robots_file();
		require(N_polygon_models == 167 && Polygon_models[166].rad == 2 * F1_0, "HAM/V-HAM restored before initial verification");
		require(N_ObjBitmaps == 422, "exit bitmap cleanup preserves mission bitmap count");
	}
	require(PHYSFS_delete("steel1.bbm") != 0, "remove fixture to exercise failed exit load");
	require(load_exit_models() == 0 && Robot_replacements_loaded, "failed exit still marks discarded assets dirty");
	load_level_robots_file("plain.rl2");
	require(Objects[2].size == 2 * F1_0 && Polygon_models[166].model_data, "recover after failed exit load");
	require(Gamesave_num_org_robots == 1 && Gamesave_num_players == 4, "refresh preserves counts");
	require(std::memcmp(&player_before, &Objects[0], sizeof(object)) == 0 && std::memcmp(&powerup_before, &Objects[1], sizeof(object)) == 0, "refresh preserves players and powerups");
	require(PowerupsInMine[POW_ENERGY] == 5 && MaxPowerupsAllowed[POW_ENERGY] == 8, "refresh preserves network accounting");
	require(Objects[2].lifeleft == 123 && Objects[2].shields == 45 * F1_0, "refresh preserves robot runtime state");
	Current_mission = nullptr;
	free_polygon_models();
	bm_free_extra_objbitmaps();
}
#else
static void test_hx1()
{
	bytes data = hxm_header();
	append_int(data, 0); // robots
	append_int(data, 0); // joints
	append_int(data, 1);
	append_int(data, 0);
	append(data, model_record(7 * F1_0));
	append(data, bytes(2));
	append_int(data, 12);
	append_int(data, 13);
	append_int(data, 1); // trailing bitmap proves the reader stayed aligned
	append_int(data, 0);
	append(data, { 17, 0 });
	write_fixture("custom.hx1", data);
	char name[] = "custom.hx1";
	load_hxm(name);
	require(Polygon_models[0].rad == 7 * F1_0 && Polygon_models[0].model_data_size == 2, "HX1 disk model decoded independent of host ABI");
	require(Dying_modelnums[0] == 12 && Dead_modelnums[0] == 13 && ObjBitmaps[0].index == 17, "HX1 trailing records aligned");
	N_polygon_models = 1;
	free_polygon_models();
}
#endif

int main(int argc, char **argv)
{
	(void) argc;
	mem_init();
	require(PHYSFS_init(argv[0]) != 0, "initialize PhysFS");
	require(PHYSFS_setWriteDir(".") != 0 && PHYSFS_mount(".", nullptr, 1) != 0, "mount isolated fixture directory");
	std::fprintf(stderr, "Testing model texture seek\n");
	test_seek();
	std::fprintf(stderr, "Testing life limits\n");
	test_lives();
#ifdef DXX_BUILD_DESCENT_II
	std::fprintf(stderr, "Testing mission robot reload\n");
	test_robot_reload();
#else
	std::fprintf(stderr, "Testing HX1 model loading\n");
	test_hx1();
#endif
	PHYSFS_deinit();
	std::puts("Upstream compatibility integration tests passed");
	return 0;
}
