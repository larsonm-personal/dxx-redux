// Android automation for the explicitly labeled message integration fixture
#include "level_message_probe.h"
#include "level_messages.h"
#include "preview.h"
#include "object_trigger.h"
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gameseq.h"
#include "object.h"
#include "player.h"
#include "switch.h"
#include "wall.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
void obj_delete_all_that_should_be_dead(void);
}
namespace
{
std::string failure, identity;
fix energy = 0;
int version = 0;
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
xl_message_state state()
{
	xl_message_state result{};
	xl_message_state_get(&result);
	return result;
}
void fixture()
{
	require(xl_level_extensions_active() && Num_triggers == 10 && xl_object_trigger_count() == 2 && Triggers[0].type == 29 && Triggers[0].value == F1_0 + F1_0 / 2 && Triggers[3].time == -1, "Message controls require the labeled derived fixture");
}
void expect(int id, int seconds, const char *text)
{
	const auto actual = state();
	require(actual.id == id && actual.duration == seconds && !std::strcmp(xl_message_text(), text), "Android message ID, duration or text differs");
}
void verify()
{
	expect(10, 100, "Repeat message");
	const auto actual = state();
	require(identity == xl_message_identity() && xl_source_level_version() == version && Players[Player_num].energy == energy && actual.elapsed_ms >= 5000 && actual.elapsed_ms < 45000, "Android message restore lost resource, earned phase or player state");
}
} // namespace
const char *xl_level_message_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Message controls require running singleplayer");
		if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && !*xl_message_text() && !*xl_message_identity() && !state().id, "Message resources leaked into Redux");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!identity.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android messages");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored message controls");
		} else {
			fixture();
			if (!std::strcmp(operation, "wall")) {
				xl_wall_trigger_reset();
				check_trigger(&Segments[Walls[0].segnum], static_cast<short>(Walls[0].sidenum), static_cast<short>(Players[Player_num].objnum), 0);
				expect(1, 100, "Wall message\nSecond line");
			} else if (!std::strcmp(operation, "replace")) {
				require(check_trigger_sub(1, Player_num, 1) == 0 && (Triggers[1].flags & TF_DISABLED), "Android message one-shot failed");
				expect(2, 3, "Replacement");
			} else if (!std::strcmp(operation, "short")) {
				require(check_trigger_sub(2, Player_num, 1) == 0, "Android short message dispatch failed");
				expect(3, 1, "Short message");
			} else if (!std::strcmp(operation, "modal")) {
				require(!game_is_time_paused(), "Modal message control must begin in running gameplay");
				const auto before = xl_message_modal_shows();
				const auto clock = GameTime64;
				require(check_trigger_sub(3, Player_num, 1) == 0 && xl_message_modal_shows() == before + 1 && !game_is_time_paused() && GameTime64 == clock, "Android modal did not pause/resume the simulation");
				expect(0, 0, "");
			} else if (!std::strcmp(operation, "object")) {
				int owner = -1;
				for (int i = 0; i < xl_object_trigger_count(); ++i) {
					xl_object_trigger_state entry{};
					require(xl_object_trigger_state_get(i, &entry), "Cannot read Android message owner");
					if (entry.value == 11 * F1_0) owner = entry.owner;
				}
				require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Android message removal owner missing");
				Objects[owner].flags |= OF_SHOULD_BE_DEAD;
				obj_delete_all_that_should_be_dead();
				expect(11, 100, "Collected object message");
				require(Objects[owner].type == OBJ_NONE, "Android message owner was not removed");
			} else if (!std::strcmp(operation, "prepare")) {
				require(game_is_time_paused(), "Message persistence controls require paused gameplay");
				xl_wall_trigger_reset();
				require(check_trigger_sub(9, Player_num, 1) == 0, "Cannot earn Android message state");
				const xl_message_state earned{ 5000, 10, 100 };
				require(xl_message_restore(&earned, 1), "Cannot establish controlled message age");
				identity = xl_message_identity();
				version = xl_source_level_version();
				energy = Players[Player_num].energy;
				verify();
				for (const char *bad : { "nomsg.rl2", "badmsg.rl2", "badid.rl2", "badsnd.rl2" }) {
					require(!xl_level_supported(bad, 0), "Android unsupported message/sound dependency admitted");
					verify();
				}
			} else if (!std::strcmp(operation, "verify")) verify();
			else if (!std::strcmp(operation, "disturb")) {
				verify();
				Players[Player_num].energy -= F1_0;
				const xl_message_state empty{};
				require(xl_message_restore(&empty, 1) && !state().id, "Message disturbance did not clear display state");
			} else if (!std::strcmp(operation, "disk_save")) {
				verify();
				require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL message phase", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android messages");
			} else if (!std::strcmp(operation, "next")) {
				require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Message transition requires the running two-level fixture");
				StartNewLevelSub(2, 1, 0);
				require(Current_level_num == 2 && xl_source_level_version() == 27 && state().id != 10, "Message transition retained stale display state");
			} else require(false, "Unknown message control operation");
		}
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
