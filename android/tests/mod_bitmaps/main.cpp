#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "android_mod_bitmaps.h"
extern "C" {
#include "args.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "piggy.h"
#include "palette.h"
#include "physfsx.h"
#include "u_mem.h"
#ifdef DXX_BUILD_DESCENT_II
#include "effects.h"
#include "polyobj.h"
#include "textures.h"
#include "wall.h"
void piggy_init_pigfile(char *name);
void free_bitmap_replacements(void);
extern char Current_level_palette[];
#else
#include "custom.h"
void custom_remove(void);
#endif
}
using nlohmann::json;
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#include "source_checks.hpp"
static std::string read(const std::string &name)
{
	auto *file = PHYSFSX_openReadBuffered(name.c_str());
	require(file != nullptr, "read fixture input");
	std::string bytes(static_cast<size_t>(PHYSFS_fileLength(file)), '\0');
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "read all input bytes");
	PHYSFS_close(file);
	return bytes;
}
static void write(const std::string &name, const std::string &bytes)
{
	auto *file = PHYSFS_openWrite(name.c_str());
	require(file && PHYSFS_writeBytes(file, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_close(file), "write fixture input");
}
#ifdef DXX_BUILD_DESCENT_II
static std::string metadata()
{
	std::string bytes;
	auto append = [&bytes](const void *data, size_t size) { bytes.append(static_cast<const char *>(data), size); };
	append(&NumTextures, sizeof(NumTextures));
	append(Textures, NumTextures * sizeof(*Textures));
	append(TmapInfo, NumTextures * sizeof(*TmapInfo));
	append(&Num_vclips, sizeof(Num_vclips));
	append(Vclip, Num_vclips * sizeof(*Vclip));
	append(&Num_effects, sizeof(Num_effects));
	append(Effects, Num_effects * sizeof(*Effects));
	append(&Num_wall_anims, sizeof(Num_wall_anims));
	for (int i = 0; i < Num_wall_anims; ++i) {
		auto wall = WallAnims[i];
		const size_t length = strnlen(wall.filename, sizeof(wall.filename));
		std::memset(wall.filename + length, 0, sizeof(wall.filename) - length);
		append(&wall, sizeof(wall));
	}
	return bytes;
}
#endif
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	GameArg.SndNoSound = GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	const int content_game = 2;
	const char *game = "d2", *pack = "uud2tp-textures.dxa";
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
	write("descent.pig", read("D2DESCENT.PIG"));
#else
	const int content_game = 1;
	const char *game = "d1", *pack = "uud1tp-textures.dxa";
#endif
	json trace = json::array();
	const auto expected = json::parse(read("uud-indexed-expectations.json")).at(game);
	require(android_mod_bitmaps_apply(content_game, "groupa.pig") == 0, "absent mod is inert");
	trace.push_back({ { "case", "absent_mod" } });
	require(PHYSFS_mount(pack, nullptr, 0), "mount converted UUD archive");
	properties_init();
#ifdef DXX_BUILD_DESCENT_II
	const auto converted_metadata = metadata();
	require(PHYSFS_unmount(pack), "unmount converted metadata for independent source read");
	auto *source_ham = PHYSFSX_openReadBuffered("UUD2.HAM");
	require(source_ham && PHYSFS_seek(source_ham, 8), "open author's complete HAM");
	free_polygon_models();
	init_polygon_models();
	bm_read_all(source_ham);
	PHYSFS_close(source_ham);
	require(converted_metadata == metadata(), "all texture, vclip, eclip and wall metadata matches author's HAM");
	require(PHYSFS_mount(pack, nullptr, 0), "remount converted metadata");
	piggy_load_mission_data();
#endif
	int checked = 0;
	std::string current;
	json rows;
	for (auto it = expected.begin(); it != expected.end(); ++it) {
		current = it.key();
		rows = it.value();
#ifdef DXX_BUILD_DESCENT_II
		/* First initialization followed by real palette switches */
		std::string pig = current + ".pig";
		if (!checked) piggy_init_pigfile(pig.data());
		else piggy_new_pigfile(pig.data());
		std::strcpy(Current_level_palette, (current + ".256").c_str());
		require(Num_bitmap_files == 2620, "HAM extra references survive every palette switch");
#else
		require(android_mod_bitmaps_apply(1, nullptr) == static_cast<int>(rows.size()), "apply D1 indexed patch");
#endif
		check_source_bitmaps(rows);
		checked += static_cast<int>(rows.size());
	}
	trace.push_back({ { "case", "all_source_pixels" }, { "palettes", expected.size() }, { "bitmaps", checked } });
	for (int i = 0; i < 256; ++i) {
		gr_palette[i * 3] = 1;
		gr_palette[i * 3 + 1] = 2;
		gr_palette[i * 3 + 2] = 3;
	}
	android_mod_bitmaps_refresh_palette();
	const auto &colored = GameBitmaps[rows.front().at("index").get<int>()];
	require(colored.avg_color_rgb[0] > 0 && colored.avg_color_rgb[1] == colored.avg_color_rgb[0] * 2 && colored.avg_color_rgb[2] == colored.avg_color_rgb[0] * 3, "resident RGB metadata uses the new palette");
	piggy_bitmap_page_out_all();
	check_source_bitmaps(rows);
	trace.push_back({ { "case", "page_out_keeps_mod_pixels" } });
	std::vector<int> flags(MAX_BITMAP_FILES);
	require(piggy_read_level_bitmap_flags("fixture.rdl", flags.data(), flags.size()), "semantic flags query");
	for (const auto &row : rows) require(flags[row.at("index").get<int>()] == row.at("flags"), "semantic query matches source flags");
	trace.push_back({ { "case", "semantic_flags" } });
	/* Author a tiny mission replacement over a slot owned by UUD */
	const int index = rows.back().at("index");
	std::string pog(32, '\0');
	std::memcpy(pog.data(), "DPOG", 4);
	pog[4] = pog[8] = 1;
	pog[12] = index & 255;
	pog[13] = (index >> 8) & 255;
	pog[23] = pog[24] = 2;
	pog[26] = 1;
	pog += std::string("\x01\xff\x02\x03", 4);
#ifdef DXX_BUILD_DESCENT_II
	write("fixture.POG", pog);
	char level[] = "fixture.rl2";
	load_bitmap_replacements(level);
#else
	write("fixture.pg1", pog);
	char level[] = "fixture.rdl";
	load_custom_data(level);
#endif
	require(GameBitmaps[index].bm_w == 2 && GameBitmaps[index].bm_data[1] == 255 && GameBitmaps[index].bm_flags == 1, "mission override wins over UUD pixels and flags");
	require(piggy_read_level_bitmap_flags(level, flags.data(), flags.size()) && flags[index] == 1, "mission semantic query wins over UUD");
	trace.push_back({ { "case", "mission_override" } });
#ifdef DXX_BUILD_DESCENT_II
	std::string pig = current + ".pig";
	piggy_new_pigfile(pig.data());
	free_bitmap_replacements();
#else
	custom_remove();
#endif
	check_source_bitmaps(rows);
	trace.push_back({ { "case", "mission_unload_restores_mod" } });
	const auto patch = content_game == 1 ? "patches/d1/bitmaps.pog" : "patches/d2/sets/" + current + ".pog";
	auto broken = read(patch);
	broken.back() ^= 1;
	/* Invalid trailing bytes must reject all mutations, including earlier valid rows */
	broken.push_back('\0');
	require(PHYSFS_mkdir(content_game == 1 ? "override/patches/d1" : "override/patches/d2/sets"), "create override directory");
	write("override/" + patch, broken);
	require(PHYSFS_mount("override", nullptr, 0), "mount invalid overlay first");
	require(android_mod_bitmaps_apply(content_game, (current + ".pig").c_str()) == -1, "reject malformed indexed overlay");
	check_source_bitmaps(rows);
	require(PHYSFS_unmount("override"), "remove invalid overlay");
	trace.push_back({ { "case", "atomic_rejection" } });
	android_mod_bitmaps_reset();
	require(PHYSFS_unmount(pack), "disable pack");
	require(android_mod_bitmaps_apply(content_game, (current + ".pig").c_str()) == 0, "disabled patch absent");
#ifdef DXX_BUILD_DESCENT_II
	piggy_load_mission_data();
	char original_pig[] = "groupa.pig";
	piggy_new_pigfile(original_pig);
	require(Num_bitmap_files == 2590, "disabled HAM no longer registers stale extra bitmap slots");
#endif
	const int stock_index = rows.front().at("index");
	require(piggy_bitmap_get_offset(stock_index) > 0, "reset restores original PIG ownership");
	bitmap_index stock = { static_cast<ushort>(stock_index) };
	piggy_bitmap_page_in(stock);
	require(GameBitmaps[stock_index].bm_data != nullptr, "original bitmap pages in after disable");
	trace.push_back({ { "case", "disable_restores_original" } });
	write(argv[1], trace.dump(2) + "\n");
	gamedata_close();
	PHYSFS_deinit();
	return 0;
}
