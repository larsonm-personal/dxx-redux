#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>
#include <nlohmann/json.hpp>
#include "mod_save_compat.h"
#include "android_d1_mod_patches.h"
extern "C" {
#include "args.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "effects.h"
#include "gameseq.h"
#include "game.h"
#include "mission.h"
#include "multi.h"
#include "piggy.h"
#include "textures.h"
#include "u_mem.h"
#include "wall.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2_assets.h"
#endif
}
using nlohmann::json;
static json trace = json::array();
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s (%s)\n", message, mod_save_reason());
		std::exit(1);
	}
}
using Record = std::array<int, 7>;
static Record record(int index, int flags = 0, int segment = -1, int texture = -99999)
{
	return { index, Effects[index].vc.frame_time, 0, flags, segment, 0, texture == -99999 ? Effects[index].dest_bm_num : texture };
}
static void world()
{
	Game_mode = 0;
	Current_level_num = 1;
	Highest_segment_index = 0;
	Num_segments = 1;
	std::memset(Segments, 0, sizeof(Segments[0]));
	Num_open_doors = Num_walls = 0;
	Highest_object_index = 0;
	Objects[0].type = OBJ_NONE;
	reset_special_effects_to_time(0);
}
static void capture()
{
	mod_save_capture_definitions();
	mod_save_capture_level("DESCENT.PIG");
}
static void save(const std::vector<Record> &records, bool legacy = false, const char *name = "fixture.sg0")
{
	rewind_file file;
	rewind_file_init_physfs(&file, PHYSFS_openWrite(name));
	require(file.physfs != nullptr, "open fixture save");
	const int version =
#ifdef DXX_BUILD_DESCENT_II
	    legacy ? 41 : 43;
#else
	    legacy ? 19 : 20;
#endif
	rewind_file_write(&file, "DGSS", 1, 4);
	rewind_file_write(&file, &version, 4, 1);
	mod_save_effect_offset(&file);
	const int header[] = { 123456, 0, int(records.size()) };
	rewind_file_write(&file, header, 4, 3);
	for (const auto &row : records) rewind_file_write(&file, row.data(), 4, row.size());
	if (!legacy) require(mod_save_write(&file), "write source metadata");
	require(rewind_file_close(&file), "close fixture save");
}
static bool restore(bool legacy = false, bool preflight_only = false, const char *name = "fixture.sg0")
{
	rewind_file file;
	rewind_file_init_physfs(&file, PHYSFS_openRead(name));
	require(file.physfs != nullptr, "read fixture save");
	rewind_file_seek(&file, 8);
	const int version =
#ifdef DXX_BUILD_DESCENT_II
	    legacy ? 41 : 43;
#else
	    legacy ? 19 : 20;
#endif
	bool ok = mod_save_prepare(&file, 1, version, 0);
	if (ok && !preflight_only) {
		ok = mod_save_validate_effects(&file, 0);
		if (ok) {
			rewind_file_seek(&file, 8);
			mod_save_read_effects(&file, 0, 1);
			ok = mod_save_adapt_world();
		}
	}
	rewind_file_close(&file);
	return ok;
}
struct Definitions {
	std::vector<eclip> effects;
	std::vector<wclip> walls;
	int textures;
	Definitions() : effects(Effects, Effects + Num_effects), walls(WallAnims, WallAnims + Num_wall_anims), textures(NumTextures) {}
	void use() const
	{
		Num_effects = int(effects.size());
		Num_wall_anims = int(walls.size());
		NumTextures = textures;
		std::copy(effects.begin(), effects.end(), Effects);
		std::copy(walls.begin(), walls.end(), WallAnims);
		capture();
	}
};
static void door()
{
	Num_walls = Num_open_doors = 1;
	Walls[0].clip_num = 9;
	Walls[0].state = WALL_DOOR_OPENING;
	ActiveDoors[0].n_parts = 1;
	ActiveDoors[0].front_wallnum[0] = 0;
	ActiveDoors[0].back_wallnum[0] = -1;
	ActiveDoors[0].time = WallAnims[9].play_time / 2;
}
static void passed(const char *name)
{
	trace.push_back({ { "case", name }, { "adapted", mod_save_adapted_count() } });
}
static void damage_save(bool truncate)
{
	auto *source = PHYSFS_openRead("fixture.sg0");
	std::vector<unsigned char> bytes(PHYSFS_fileLength(source));
	require(PHYSFS_readBytes(source, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "read damage fixture");
	PHYSFS_close(source);
	if (truncate) bytes.resize(bytes.size() - 1);
	else bytes[bytes.size() - 80] ^= 1;
	auto *destination = PHYSFS_openWrite("fixture.sg0");
	require(PHYSFS_writeBytes(destination, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "write damage fixture");
	PHYSFS_close(destination);
}
int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	require(PHYSFS_mount("DESCENT.HOG", nullptr, 1), "D1 palette");
	GameArg.SndNoSound = GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = 22050;
#endif
	digi_select_system(SDLAUDIO_SYSTEM);
	properties_init();
	require(Num_effects > 3 && Num_wall_anims > 9, "load real stock definitions");
	world();
	capture();
	const Definitions stock;
	save({ record(3) });
	require(restore() && mod_save_adapted_count() == 0, "stock exact roundtrip");
	passed("stock_exact");
#ifdef DXX_BUILD_DESCENT_II
	mod_save_capture_extra_ham("DESCENT.PIG");
	save({ record(3) });
	require(restore(false, true), "unchanged additional gameplay source accepted");
	capture();
	require(!restore(false, true), "changed additional gameplay source rejected before mutation");
	passed("mission_gameplay_identity");
#endif
	save({ record(3) });
	damage_save(false);
	require(!restore(false, true), "source checksum mismatch rejected");
	passed("source_checksum");
	save({ record(3) });
	damage_save(true);
	require(!restore(false, true), "truncated mandatory source block rejected");
	passed("source_truncated");
	auto bad = record(3);
	bad[2] = 1000;
	save({ bad });
	require(!restore(false, true), "invalid frame rejected before world load");
	passed("invalid_frame");
	save({ record(3), record(3) });
	require(!restore(false, true), "duplicate rejected before world load");
	passed("duplicate_effect");
	save({ record(3, EF_ONE_SHOT) });
	require(!restore(false, true), "one-shot without target rejected");
	passed("missing_oneshot_target");
#ifdef DXX_BUILD_DESCENT_II
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "enable UUD2TP");
	piggy_load_mission_data();
	world();
	capture();
	require(Num_effects == 107 && Effects[40].vc.num_frames == 3 && NumTextures > 924, "actual UUD2TP definitions");
	const Definitions modded;
	save({ record(25), record(40), record(76), record(105), record(106) });
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable without deleting UUD2TP");
	stock.use();
	Game_mode = GM_MULTI;
	require(!restore(false, true), "coop requires exact new source definitions");
	Game_mode = 0;
	passed("coop_source_identity");
	require(restore() && mod_save_adapted_count() == 5, "obsolete loops safely rebuilt or dropped");
	passed("uud2tp_disabled");
	save({ record(25), record(40), record(76) });
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "re-enable UUD2TP");
	modded.use();
	require(restore(), "stock save with UUD2TP enabled");
	passed("uud2tp_enabled");
	world();
	capture();
	door();
	Segments[0].sides[0].tmap_num2 = short(910 | 0x4000);
	save({ record(40), record(105), record(106) });
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable for destroyed monitor");
	stock.use();
	require(restore() && Segments[0].sides[0].tmap_num2 == (373 | 0x4000), "destroyed monitor and orientation preserved");
	require(std::abs(ActiveDoors[0].time - WallAnims[9].play_time / 2) <= 1, "opening door phase preserved");
	passed("destroyed_monitor_and_door");
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "enable for one-shot");
	modded.use();
	world();
	Effects[40].flags |= EF_ONE_SHOT;
	Segments[0].sides[0].tmap_num2 = short(911 | 0x4000);
	save({ record(38), record(40, EF_ONE_SHOT, 0, 910) });
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable for one-shot");
	stock.use();
	require(restore(), "active one-shot remaps without normal-loop collision");
	require(Effects[38].segnum == 0 && (Effects[38].flags & EF_ONE_SHOT) && Effects[38].dest_bm_num == 373 && Segments[0].sides[0].tmap_num2 == (386 | 0x4000), "one-shot world target preserved");
	passed("monitor_oneshot");
	FrameTime = 4 * Effects[38].vc.frame_time;
	do_special_effects();
	require(Effects[38].segnum == -1 && Segments[0].sides[0].tmap_num2 == (373 | 0x4000), "mapped one-shot completes monitor destruction");
	world();
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "enable for extra texture");
	modded.use();
	Segments[0].sides[0].tmap_num = 923;
	save({ record(105) });
	Segments[0].sides[0].tmap_num = 0;
	Objects[0].type = OBJ_ROBOT;
	Objects[0].render_type = RT_POLYOBJ;
	Objects[0].rtype.pobj_info.tmap_override = 923;
	save({}, false, "extra-object.sg0");
	Objects[0].type = OBJ_NONE;
	Segments[0].sides[0].tmap_num = 923;
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable extra texture source");
	stock.use();
	require(!restore(false, true) && Segments[0].sides[0].tmap_num == 923, "real extra dependency refuses before mutation");
	passed("extra_texture_rejected");
	require(!restore(false, true, "extra-object.sg0"), "extra model texture dependency rejected before mutation");
	passed("extra_object_texture_rejected");
	world();
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "enable for companion");
	modded.use();
	Segments[0].sides[0].tmap_num = 924;
	save({ record(106) }, false, "companion.sg0");
	Segments[0].sides[0].tmap_num = 0;
	save({ record(40) });
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable companion source");
	stock.use();
	require(restore(false, true) && !mod_save_check_companion("companion.sg0"), "incompatible companion rejects pair");
	save({}, false, "stock-companion.sg0");
	require(restore(false, true) && mod_save_check_companion("stock-companion.sg0"), "compatible companion preserves primary restore policy");
	passed("companion_dependency");
	world();
	require(PHYSFS_mount("uud2tp-textures.dxa", nullptr, 0), "enable for legacy");
	modded.use();
	save({ record(40), record(105), record(106) }, true);
	require(PHYSFS_unmount("uud2tp-textures.dxa"), "disable for legacy");
	stock.use();
	require(restore(true), "released UUD2 sparse records recover");
	passed("legacy_uud2tp");
	Game_mode = GM_MULTI;
	require(!restore(true), "coop refuses legacy adaptation");
	Game_mode = 0;
	passed("coop_exact_policy");
	world();
	require(PHYSFS_mount("uud2sp-sounds.dxa", nullptr, 0), "enable UUD2SP");
	piggy_load_mission_data();
	capture();
	door();
	save({ record(3) });
	require(PHYSFS_unmount("uud2sp-sounds.dxa"), "disable UUD2SP");
	stock.use();
	require(restore() && std::abs(ActiveDoors[0].time - WallAnims[9].play_time / 2) <= 1, "sound patch effect and door timing adapt");
	passed("uud2sp_timing");
	static Mission d1_mission;
	d1_mission.descent_version = 1;
	Current_mission = &d1_mission;
	const char *error = nullptr;
	auto *assets = d1_in_d2_read_assets("DESCENT.PIG", "palette.256", &error);
	require(assets && d1_in_d2_publish_assets(assets, &error), error ? error : "native D1 in D2 assets");
#endif
	world();
	capture();
	const Definitions d1_stock;
	const char *mount =
#ifdef DXX_BUILD_DESCENT_II
	    "d1-mods";
#else
	    nullptr;
#endif
	require(PHYSFS_mount("uud1tp-textures.dxa", mount, 0), "enable UUD1");
	require(android_d1_mod_apply_properties() == 1, "real UUD1 property patch");
	capture();
	door();
	save({ record(3) });
	require(PHYSFS_unmount("uud1tp-textures.dxa"), "disable UUD1 installed archive");
	d1_stock.use();
	require(restore() && std::abs(ActiveDoors[0].time - WallAnims[9].play_time / 2) <= 1, "D1 door phase preserved across toggle");
	passed("uud1_door_phase");
	std::FILE *output = std::fopen(argv[1], "wb");
	const auto text = trace.dump(2) + "\n";
	require(output && std::fwrite(text.data(), 1, text.size(), output) == text.size(), "write normalized trace");
	std::fclose(output);
	return 0;
}
