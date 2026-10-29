// Android automation controls for the labeled canonical-layout fixture
#include "runtime_layout_probe.h"
#include "preview.h"
#include "changing_light.h"
#include "trigger_state.h"
#include "resource_validation.h"
#include "../inventory.h"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "object.h"
#include "player.h"
#include "mission.h"
#include "physfsx.h"
#include "switch.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
}
namespace
{
std::string failure;
std::uint64_t identity = 0;
int version = 0;
fix energy = 0;
xl_light_index light{};
std::vector<xl_light_segment_state> earned;
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
std::vector<xl_light_segment_state> lights()
{
	std::vector<xl_light_segment_state> result(xl_light_segment_count());
	for (std::size_t i = 0; i < result.size(); ++i) require(xl_light_segment_get(static_cast<int>(i), &result[i]), "Cannot capture modern light consumers");
	return result;
}
bool same(const std::vector<xl_light_segment_state> &a, const std::vector<xl_light_segment_state> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}
void verify()
{
	require(xl_level_extensions_active() && identity && xl_level_trigger_identity() == identity && xl_source_level_version() == version && xl_source_game_version() == 40 && Players[Player_num].energy == energy && same(earned, lights()), "Android modern restore lost layout, identity, energy or light consumers");
	int brightness = -1;
	require(xl_prepared_texture_brightness(302, &brightness) && brightness == (version - 22) * 16384, "Android modern restore lost owned companion brightness");
}
void prepare()
{
	version = xl_source_level_version();
	require((version == 25 || version == 27) && xl_source_game_version() == 40 && game_is_time_paused(), "Modern controls require a paused 25/40 or 27/40 level");
	require(mission_owns_resource_archive(PHYSFS_getRealDir(Gamesave_current_filename)), "Modern level is outside the selected archive");
	std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> file(PHYSFS_openRead(Gamesave_current_filename), PHYSFS_close);
	require(file && PHYSFS_fileLength(file.get()) > 0 && PHYSFS_fileLength(file.get()) < 1024 * 1024, "Modern layout source missing or outside fixture bound");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
	require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "Cannot read modern owned source");
	const auto source = xl_compat::inspect_level(bytes);
	require(source.level_version == version && source.segments.size() == std::size_t(Num_segments) && source.vertices.size() == std::size_t(Num_vertices) && source.segments.size() == 93 && source.fog_settings.size() == (version == 27 ? 4u : 0u), "Android layout source counts differ");
	for (std::size_t i = 0; i < source.vertices.size(); ++i)
		require(Vertices[i].x == source.vertices[i][0] && Vertices[i].y == source.vertices[i][1] && Vertices[i].z == source.vertices[i][2], "Android modern vertex changed");
	for (std::size_t i = 0; i < source.segments.size(); ++i) {
		for (int slot = 0; slot < 8; ++slot) require(Segments[i].verts[slot] == source.segments[i].vertices[slot], "Android modern segment vertex changed");
		for (int side = 0; side < 6; ++side) {
			const auto &expected = source.segments[i].sides[side];
			const auto &actual = Segments[i].sides[side];
			require(expected.corners == xl_compat::classic_side_corners[side] && Segments[i].children[side] == source.segments[i].children[side] && actual.wall_num == expected.wall, "Android explicit side topology changed");
			if (expected.texture < 0) continue;
			require(actual.tmap_num == expected.texture && actual.tmap_num2 == expected.overlay, "Android modern texture/orientation changed");
			for (int corner = 0; corner < 4; ++corner)
				require(actual.uvls[corner].u == expected.uvl[corner * 3] * 32 && actual.uvls[corner].v == expected.uvl[corner * 3 + 1] * 32, "Android modern UV coordinate changed");
		}
	}
	require(xl_light_index_get(0, &light), "Modern fixture has no changing-light source");
	add_light(light.segment, light.side);
	const auto before = lights();
	require(subtract_light(light.segment, light.side) == 1, "Cannot earn modern changed-light state");
	earned = lights();
	require(!same(before, earned), "Modern fixture did not change actual light consumers");
	identity = xl_level_trigger_identity();
	energy = Players[Player_num].energy;
	verify();
	for (const char *bad : { "badside.rl2", "badcorn.rl2", "badprop.rl2" }) {
		require(!xl_level_supported(bad, 0), "Android modern geometry/fog gate bypassed");
		verify();
	}
}
} // namespace
const char *xl_runtime_layout_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Modern controls require running singleplayer");
		if (!std::strcmp(operation, "prepare")) prepare();
		else if (!std::strcmp(operation, "verify")) verify();
		else if (!std::strcmp(operation, "disturb")) {
			verify();
			Players[Player_num].energy -= F1_0;
			require(add_light(light.segment, light.side) == 1 && !same(earned, lights()), "Modern disturbance did not change light consumers");
		} else if (!std::strcmp(operation, "disk_save")) {
			verify();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL modern layout", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android modern layout");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(identity && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android modern layout");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored modern layout");
		} else if (!std::strcmp(operation, "next")) {
			require(!game_is_time_paused() && Current_level_num >= 1 && Current_level_num < Last_level, "Modern transition requires an active next level");
			const int destination = Current_level_num + 1;
			StartNewLevelSub(destination, 1, 0);
			require(Current_level_num == destination && xl_level_extensions_active(), "Modern normal level transition failed");
		} else if (!std::strcmp(operation, "stock")) {
			int brightness = 0;
			require(!xl_level_extensions_active() && !xl_prepared_texture_brightness(302, &brightness) && !xl_light_segment_count(), "Modern layout resources leaked into stock");
		} else require(false, "Unknown modern layout operation");
		return nullptr;
	} catch (const std::exception &e) {
		failure = e.what();
		return failure.c_str();
	}
}
