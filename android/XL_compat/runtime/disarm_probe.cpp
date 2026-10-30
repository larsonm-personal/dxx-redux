// Android controlled attack consumers and ordinary disk/rewind persistence
#include "disarm_probe.h"
#include "disarm_controls.h"
extern "C" {
#include "state_android_shared.h"
#include "android_meta_actions.h"
}
namespace
{
using xl_disarm_controls::require;
std::string failure, report = "null";
nlohmann::json earned;
xl_trigger_state pending{};
int version = 0, energy = 0;
unsigned verifies = 0, events_checked = 0, consumers_checked = 0, lifetimes_checked = 0, stock_checked = 0;
void verify()
{
	require(version && xl_level_extensions_active() && xl_source_level_version() == version && Players[Player_num].energy == energy && xl_disarm_controls::snapshot() == earned, "Android restore lost disarmed robot lifetimes or player state");
	xl_trigger_state current{};
	require(xl_wall_trigger_state(2, &current) && current.elapsed == pending.elapsed && current.delay_ms == pending.delay_ms && current.operated, "Android disarm restore lost the pending delayed action");
	++verifies;
}
} // namespace
const char *xl_disarm_report()
{
	return report.c_str();
}
const char *xl_disarm_probe(const char *operation)
{
	try {
		using namespace xl_disarm_controls;
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Disarm controls require running singleplayer");
		if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && snapshot().empty(), "Disarmed state survived Redux transition");
			consumers(true);
			++stock_checked;
		} else if (!std::strcmp(operation, "disk_load")) {
			require(version && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android disarm state");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored disarm state");
		} else {
			fixture();
			if (!std::strcmp(operation, "events")) {
				events();
				lifetimes();
				++events_checked;
				++lifetimes_checked;
			} else if (!std::strcmp(operation, "consumers")) {
				consumers();
				++consumers_checked;
			} else if (!std::strcmp(operation, "prepare")) {
				require(game_is_time_paused(), "Disarm capture requires paused gameplay");
				pending = prepare();
				earned = snapshot();
				version = xl_source_level_version();
				energy = Players[Player_num].energy;
				verify();
				require(!xl_level_supported("badtarget.rl2", 0), "Android admitted unsupported robot reprogramming");
				verify();
			} else if (!std::strcmp(operation, "verify")) verify();
			else if (!std::strcmp(operation, "disturb")) {
				verify();
				continuation(pending);
				Players[Player_num].energy -= F1_0;
				require(snapshot() != earned, "Disarm continuation failed to change attack state");
			} else if (!std::strcmp(operation, "disk_save")) {
				verify();
				require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL disarmed robot continuation", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android disarm state");
			} else if (!std::strcmp(operation, "next")) {
				require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Disarm transition requires the running two-level fixture");
				StartNewLevelSub(2, 1, 0);
				require(Current_level_num == 2 && xl_source_level_version() == 27, "Disarm transition did not select the next layout");
			} else require(false, "Unknown disarm control operation");
		}
		report = nlohmann::json{ { "passed", true }, { "verifies", verifies }, { "event_sequences", events_checked }, { "consumer_sequences", consumers_checked }, { "lifetime_sequences", lifetimes_checked }, { "stock_sequences", stock_checked } }.dump();
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
