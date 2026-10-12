#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "android_d1_mod_patches.h"
#include "android_mod_bitmaps.h"
extern "C" {
#include "args.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "gamemine.h"
#include "object.h"
#include "gamesave.h"
#include "gameseg.h"
#include "physfsx.h"
#include "textures.h"
#include "u_mem.h"
#include "wall.h"
#ifndef DXX_BUILD_DESCENT_II
#include "custom.h"
void custom_remove(void);
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#include "d1_in_d2/d1_in_d2_assets.h"
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
#include "../mod_bitmaps/source_checks.hpp"
static const char *mount_point()
{
#ifdef DXX_BUILD_DESCENT_II
	return "d1-mods";
#else
	return nullptr;
#endif
}
static std::string virtual_patch(const char *name)
{
	return std::string(mount_point() ? "d1-mods/patches/d1/" : "patches/d1/") + name;
}
static std::string read(const std::string &path)
{
	auto *file = PHYSFSX_openReadBuffered(path.c_str());
	require(file != nullptr, "open fixture source");
	std::string text(static_cast<size_t>(PHYSFS_fileLength(file)), '\0');
	require(PHYSFS_readBytes(file, &text[0], text.size()) == static_cast<PHYSFS_sint64>(text.size()), "read fixture source");
	PHYSFS_close(file);
	return text;
}
static void override_patch(const char *name, const json &patch)
{
	require(PHYSFS_mkdir("override/patches/d1"), "create patch override directory");
	const std::string path = std::string("override/patches/d1/") + name;
	auto *file = PHYSFS_openWrite(path.c_str());
	const auto text = patch.dump();
	require(file && PHYSFS_writeBytes(file, text.data(), text.size()) == static_cast<PHYSFS_sint64>(text.size()) && PHYSFS_close(file), "write override");
	require(PHYSFS_mount("override", mount_point(), 0), "mount override first");
}
static std::vector<short> textures()
{
	std::vector<short> result;
	for (int s = 0; s < Num_segments; ++s)
		for (const auto &side : Segments[s].sides) {
			result.push_back(side.tmap_num);
			result.push_back(side.tmap_num2);
		}
	return result;
}
static void load_mine(const char *name)
{
	auto *file = PHYSFSX_openReadBuffered(name);
	require(file != nullptr, "open original or author-patched level");
	PHYSFSX_readInt(file);
	Gamesave_current_version = PHYSFSX_readInt(file);
	const int offset = PHYSFSX_readInt(file);
	std::strcpy(Gamesave_current_filename, name);
	require(PHYSFS_seek(file, offset), "seek compiled mine");
	require(load_mine_data_compiled(file) == 0, "decode actual compiled mine with production engine");
	PHYSFS_close(file);
}
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	require(PHYSFS_mount("DESCENT.HOG", nullptr, 1), "mount original campaign");
	GameArg.SndNoSound = GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
#endif
	digi_select_system(SDLAUDIO_SYSTEM);
	json trace = json::array();
	require(android_d1_mod_apply_properties() == 0, "absent patch is inert");
	trace.push_back({ { "case", "absent_properties" } });
	require(PHYSFS_mount("uud1tp-textures.dxa", mount_point(), 0), "mount converted UUD1 archive");
#ifdef DXX_BUILD_DESCENT_II
	require(android_d1_mod_apply_properties() == 0, "D1 patch is inert without native D1 assets");
	const char *error = nullptr;
	auto *assets = d1_in_d2_read_assets("DESCENT.PIG", "palette.256", &error);
	require(assets != nullptr, error ? error : "read registered D1 assets");
	require(d1_in_d2_publish_assets(assets, &error), error ? error : "publish D1 assets and apply properties hook");
#else
	properties_init();
	require(WallAnims[9].play_time == 104857, "read unmodified D1 source timing");
	require(android_d1_mod_apply_properties() == 1, "apply generated timing patch");
	require(android_mod_bitmaps_apply(1, nullptr) == 120, "apply D1 bitmap patch");
#endif
	const auto indexed = json::parse(read("uud-indexed-expectations.json")).at("d1").at("descent");
	check_source_bitmaps(indexed);
	trace.push_back({ { "case", "indexed_source_pixels" }, { "bitmaps", indexed.size() } });
	require(WallAnims[9].play_time == 65536, "door11 now opens in one second");
	trace.push_back({ { "case", "authored_door_timing" }, { "play_time", WallAnims[9].play_time } });
	const auto properties = json::parse(read(virtual_patch("properties_patch.rfc6902.json")));
	WallAnims[9].play_time = 104857;
	auto invalid_properties = properties;
	invalid_properties.push_back({ { "op", "test" }, { "path", "/sections/wclips/0/PlayTime" }, { "value", WallAnims[0].play_time } });
	invalid_properties.push_back({ { "op", "replace" }, { "path", "/sections/wclips/0/PlayTime" }, { "value", 0 } });
	override_patch("properties_patch.rfc6902.json", invalid_properties);
	require(android_d1_mod_apply_properties() == -1 && WallAnims[9].play_time == 104857, "late invalid timing rolls back all writes");
	require(PHYSFS_unmount("override"), "remove invalid timing override");
	trace.push_back({ { "case", "atomic_properties_rejection" } });
	init_objects();
	const auto level_patch = json::parse(read(virtual_patch("level_surface_patch.rfc6902.json")));
	for (const int level : { 3, 17, 20 }) {
		char base[32], expected[40];
		std::snprintf(base, sizeof(base), "level%02d.rdl", level);
		std::snprintf(expected, sizeof(expected), "expected-level%02d.rdl", level);
		load_mine(base);
		const auto original = textures();
		const int count = android_d1_mod_apply_level(base);
		require(count > 0 && textures() != original, "apply actual campaign surface changes");
		const auto actual = textures();
		load_mine(expected);
		require(textures() == actual, "every surface matches author's complete patched RDL");
		trace.push_back({ { "case", "authored_level" }, { "level", level }, { "changed_surfaces", count }, { "compared_texture_fields", actual.size() } });
		load_mine(base);
		auto invalid = level_patch;
		const std::string prefix = std::string("/levels/") + base + "/";
		for (auto &op : invalid)
			if (op.at("op") == "replace" && op.at("path").get<std::string>().find(prefix) == 0) {
				op["value"]["tmap1"] = NumTextures;
				break;
			}
		override_patch("level_surface_patch.rfc6902.json", invalid);
		require(android_d1_mod_apply_level(base) == -1 && textures() == original, "invalid surface rejects whole transaction");
		require(PHYSFS_unmount("override"), "remove invalid level override");
		trace.push_back({ { "case", "atomic_level_rejection" }, { "level", level } });
		auto wrong_source = level_patch;
		for (auto &op : wrong_source)
			if (op.at("path") == prefix + "sha256") op["value"] = std::string(64, '0');
		override_patch("level_surface_patch.rfc6902.json", wrong_source);
		require(android_d1_mod_apply_level(base) == -1 && textures() == original, "same filename with different source cannot be patched");
		require(PHYSFS_unmount("override"), "remove source mismatch override");
		trace.push_back({ { "case", "level_hash_rejection" }, { "level", level } });
		require(PHYSFS_unmount("uud1tp-textures.dxa"), "disable UUD pack");
		load_mine(base);
		require(android_d1_mod_apply_level(base) == 0 && textures() == original, "disable and reload restores original surfaces");
		require(PHYSFS_mount("uud1tp-textures.dxa", mount_point(), 0), "reenable UUD pack");
		trace.push_back({ { "case", "level_disable_reload" }, { "level", level } });
	}
	/* A mission PG1 is applied before publication in D1-in-D2 */
	const int custom_index = indexed.back().at("index");
	std::string pog(32, '\0');
	std::memcpy(pog.data(), "DPOG", 4);
	pog[4] = pog[8] = 1;
	pog[12] = custom_index & 255;
	pog[13] = (custom_index >> 8) & 255;
	pog[23] = pog[24] = 2;
	pog[26] = 1;
	pog += std::string("\x01\xff\x02\x03", 4);
	auto *custom_file = PHYSFS_openWrite("fixture.pg1");
	require(custom_file && PHYSFS_writeBytes(custom_file, pog.data(), pog.size()) == static_cast<PHYSFS_sint64>(pog.size()) && PHYSFS_close(custom_file), "write mission PG1");
#ifdef DXX_BUILD_DESCENT_II
	assets = d1_in_d2_read_assets("DESCENT.PIG", "palette.256", &error);
	require(assets && d1_custom_read_assets(assets, "fixture.rdl", &error), error ? error : "read mission PG1 into D1 generation");
	require(d1_in_d2_publish_assets(assets, &error), error ? error : "publish custom D1 generation");
#else
	char level[] = "fixture.rdl";
	load_custom_data(level);
#endif
	require(GameBitmaps[custom_index].bm_w == 2 && GameBitmaps[custom_index].bm_data[1] == 255, "mission custom pixels take precedence over UUD");
	auto remaining = indexed;
	remaining.erase(remaining.size() - 1);
	check_source_bitmaps(remaining);
	trace.push_back({ { "case", "mission_pixels_preserved" }, { "mod_bitmaps", remaining.size() } });
#ifndef DXX_BUILD_DESCENT_II
	custom_remove();
#endif
	FILE *file = std::fopen(argv[1], "wb");
	const auto text = trace.dump(2) + "\n";
	require(file && std::fwrite(text.data(), 1, text.size(), file) == text.size() && std::fclose(file) == 0, "write acceptance trace");
	gamedata_close();
	PHYSFS_deinit();
	return 0;
}
