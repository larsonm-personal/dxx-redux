#include "hog_probe.h"
#include "preview.h"
#include "trigger_state.h"
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "mission.h"
#include "physfsx.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
}
namespace
{
std::string failure;
std::uint64_t identity;
fix energy;
constexpr char member[] = "A-long-owned-resource-with-more-than-thirteen-characters.bin";
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
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
		if (!std::strcmp(operation, "prepare")) {
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
