#include "hog_probe.h"
#include "hog_checkpoint_test.h"
#include "preview.h"
#include "trigger_state.h"
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
constexpr char member[] = "A-long-owned-resource-with-more-than-thirteen-characters.bin";
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
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
	require(xl_level_supported_at(filename, 0, ordinal), "Original HOG level failed admission");
	if (Current_level_num != ordinal) StartNewLevelSub(ordinal, 1, 0);
	require(Current_level_num == ordinal && !dxx_hog_name_compare(Gamesave_current_filename, filename) && Num_segments > 0 && ConsoleObject, "Original HOG level failed to start");
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
