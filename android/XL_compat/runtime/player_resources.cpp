#include "../player_resources.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "player.h"
#include "object.h"
#include "game.h"
#include "switch.h"
void start_player_death_sequence(object *player);
}
extern "C" int xl_player_resource_trigger(const trigger *action)
{
	if (!xl_level_extensions_active() || !action || !xl_compat::player_resource_action(action->type)) return 0;
	if (!Player_is_dead && ConsoleObject) {
		auto &player = Players[Player_num];
		if (action->type == 17) {
			player.shields = xl_compat::player_resource_after(player.shields, action->value, false);
			ConsoleObject->shields = player.shields;
			if (player.shields < 0) start_player_death_sequence(ConsoleObject);
		} else player.energy = xl_compat::player_resource_after(player.energy, action->value, true);
	}
	return 1;
}
