#include "hog_probe.h"
#include "hog_checkpoint_test.h"
#include "original_lighting_test.h"
#include <android/log.h>
#include "preview.h"
#include "trigger_state.h"
#include "pog_resources.h"
#include "../hog_format.h"
#include <array>
#include <charconv>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "object.h"
#include "player.h"
#include "mission.h"
#include "physfsx.h"
#include "state_android_shared.h"
#include "state.h"
#include "android_meta_actions.h"
#include "effects.h"
#include "piggy.h"
#include "textures.h"
}
namespace
{
std::string failure;
nlohmann::json saved_disk, saved_memory;
struct OriginalCheckpoint {
	rewind_memory_buffer bytes{};
	~OriginalCheckpoint()
	{
		rewind_memory_buffer_discard(&bytes);
	}
} original_checkpoint;
std::uint64_t identity;
fix energy;
unsigned vignette_frames;
constexpr char member[] = "A-long-owned-resource-with-more-than-thirteen-characters.bin";
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}

void sample_vignette_effect(bool finish)
{
	require(Current_mission && !dxx_hog_name_compare(Current_mission->filename, "vignette-xl") && Current_level_num == 1 &&
	            !dxx_hog_name_compare(Gamesave_current_filename, "standoff.rl2") && Num_effects > 84,
	        "Expected original Vignette Standoff");
	const auto &effect = Effects[84];
	require(effect.changing_wall_texture == 423 && effect.vc.num_frames == 8 && effect.vc.frame_time == 3276 &&
	            xl_pog_effect_frames(&effect) == 6 && effect.frame_count >= 0 && effect.frame_count < 6,
	        "Standoff wall clock differs from the original");
	const auto &display = GameBitmaps[Textures[423].index];
	require(display.bm_type == BM_RGBA && display.bm_data && display.bm_w == 128 && display.bm_h == 128 && display.bm_rowsize == 512,
	        "Standoff display dimensions differ from the original");
	for (int frame = 0; frame < 8; ++frame) {
		const int bitmap = effect.vc.frames[frame].index;
		xl_pog_pixel_view physics{};
		require(bitmap == 2028 + frame && xl_pog_physics_view(bitmap, &physics) && physics.width == 128 && physics.height == 128 &&
		            GameBitmaps[bitmap].bm_data == display.bm_data,
		        "Standoff lost a native effect alias");
	}
	// All six values were read back from the original D2X-XL GPU textures
	constexpr unsigned expected[] = { 3850729079u, 1204652987u, 4045285609u, 3609115056u, 1588377698u, 168255134u };
	unsigned hash = 2166136261u;
	for (int byte = 0; byte < display.bm_rowsize * display.bm_h; ++byte) hash = (hash ^ display.bm_data[byte]) * 16777619u;
	require(hash == expected[effect.frame_count], "Standoff displayed pixels differ from the original");
	vignette_frames |= 1u << effect.frame_count;
	__android_log_print(ANDROID_LOG_INFO, "DXX-Wall", "phase=%d pixels=%u observed=%u", effect.frame_count, hash, vignette_frames);
	if (finish) {
		require(vignette_frames == 63, "Standoff did not display all six original frames during sampling");
		vignette_frames = 0;
	}
}
// Exercise an advertised ordinal through the existing mission admission and level loader.
void original_level(const char *request)
{
	const char *separator = std::strchr(request, ':');
	require(separator && separator[1], "Expected a signed ordinal and level filename");
	int ordinal = 0;
	const auto parsed = std::from_chars(request, separator, ordinal);
	require(parsed.ec == std::errc() && parsed.ptr == separator && ordinal && Current_mission && ordinal >= Last_secret_level && ordinal <= Last_level,
	        "HOG probe ordinal outside the current mission");
	const char *filename = ordinal < 0 ? Secret_level_names[-ordinal - 1] : Level_names[ordinal - 1];
	require(!dxx_hog_name_compare(filename, separator + 1) && mission_owns_resource_archive(PHYSFS_getRealDir(filename)), "Advertised level or owning HOG differs");
	require(!game_is_time_paused(), "Original HOG transition requires resumed gameplay");
	if (!xl_level_supported_at(filename, 0, ordinal)) throw std::runtime_error(xl_level_rejection());
	if (Current_level_num != ordinal) StartNewLevelSub(ordinal, 1, 0);
	require(Current_level_num == ordinal && !dxx_hog_name_compare(Gamesave_current_filename, filename) && Num_segments > 0 && ConsoleObject, "Original HOG level failed to start");
}

void vignette_lighting(const char *operation)
{
	if (!std::strcmp(operation, "lighting_vignette")) {
		char mission[] = "vignette-xl";
		require(load_mission_by_name(mission), "Cannot select original Vignette mission");
		original_level("6:clminate.rl2");
	}
	require(Current_mission && !std::strcmp(Current_mission->filename, "vignette-xl") && Current_level_num == 6 &&
	            xl_prepared_classic_lighting() && !xl_level_extensions_active(),
	        "Expected the native Vignette lighting original");
	if (!std::strcmp(operation, "lighting_off")) require(subtract_light(113, 1), "Cannot extinguish original variable light");
	else if (!std::strcmp(operation, "lighting_on")) require(add_light(113, 1), "Cannot restore original variable light");
	const auto sample = xl_original_lighting_face(113, 1);
	const bool extinguished = !std::strcmp(operation, "lighting_off");
	require(sample.at("subtracted") == extinguished && sample.contains("emitter") == !extinguished,
	        "Variable light and renderer emitter disagree");
	require(sample.at("brightness") == 83840 && sample.at("raw_rgb") == std::array<int, 3>{ 2054847098, 2054847098, 2054847098 } &&
	            sample.at("sources").at("d2x-xl.hog/descent2.clr") == "a0cefca9be4746f2bd271f7b21f6687e18cb4b2e7b544c7190fa127346aa454e",
	        "Original Android face has the wrong effective tables");
	__android_log_print(ANDROID_LOG_INFO, "DXX-Lighting", "%s %s", operation, sample.dump().c_str());
}
void verify()
{
	require(xl_level_extensions_active() && identity && xl_level_trigger_identity() == identity && Players[Player_num].energy == energy, "Android extended archive restore lost source identity or saved state");
	require(mission_owns_resource_archive(PHYSFS_getRealDir(member)), "Extended HOG lost owning mission identity");
	std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> file(PHYSFS_openRead(member), PHYSFS_close);
	std::array<unsigned char, 6> bytes{};
	const std::array<unsigned char, 6> expected{ 17, 31, 42, 93, 0, 255 };
	require(file && PHYSFS_fileLength(file.get()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && bytes == expected, "Extended long-name resource bytes differ");
	require(!PHYSFS_exists("alias.bin"), "Extended short-name alias leaked into lookup");
}
} // namespace
const char *xl_hog_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "HOG probe requires running singleplayer");
		if (!std::strncmp(operation, "visit:", 6)) original_level(operation + 6);
		else if (!std::strcmp(operation, "standoff_frame") || !std::strcmp(operation, "standoff_complete"))
			sample_vignette_effect(!std::strcmp(operation, "standoff_complete"));
		else if (!std::strcmp(operation, "lighting_vignette") || !std::strcmp(operation, "lighting_sample") ||
		         !std::strcmp(operation, "lighting_off") || !std::strcmp(operation, "lighting_on"))
			vignette_lighting(operation);
		else if (!std::strcmp(operation, "remember_memory")) {
			saved_memory = hog_checkpoint_observation();
			rewind_memory_buffer_discard(&original_checkpoint.bytes);
			stop_time();
			require(state_save_to_memory(&original_checkpoint.bytes, "Original mission rewind", ANDROID_SAVE_META_KIND_MANUAL, 1), "Cannot capture original mission rewind");
		} else if (!std::strcmp(operation, "restore_memory")) {
			require(!saved_memory.empty() && Current_level_num != saved_memory.at("level").get<int>(), "Rewind control must start on a different ordinal");
			require(state_restore_from_memory(&original_checkpoint.bytes) && saved_memory == hog_checkpoint_observation(), "Original mission rewind changed the saved level, assets or pilot");
			rewind_memory_buffer_discard(&original_checkpoint.bytes);
			saved_memory.clear();
		} else if (!std::strcmp(operation, "remember_disk")) {
			saved_disk = hog_checkpoint_observation();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "Original mission", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot quicksave original mission");
		} else if (!std::strcmp(operation, "restore_disk")) {
			require(!saved_disk.empty() && Current_level_num != saved_disk.at("level").get<int>(), "Quicksave control must start on a different ordinal");
			require(state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1 && saved_disk == hog_checkpoint_observation(), "Original mission quickload changed the saved level, assets or pilot");
			saved_disk.clear();
		} else if (!std::strcmp(operation, "prepare")) {
			identity = xl_level_trigger_identity();
			energy = Players[Player_num].energy;
			verify();
		} else if (!std::strcmp(operation, "verify")) verify();
		else if (!std::strcmp(operation, "disturb")) {
			verify();
			Players[Player_num].energy -= F1_0;
		} else if (!std::strcmp(operation, "disk_save")) {
			verify();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL extended archive", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android extended archive state");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(identity && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android extended archive");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored extended archive state");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && !PHYSFS_exists(member), "Extended archive state leaked into stock");
		} else require(false, "Unknown extended HOG probe operation");
		return nullptr;
	} catch (const std::exception &e) {
		failure = e.what();
		return failure.c_str();
	}
}
