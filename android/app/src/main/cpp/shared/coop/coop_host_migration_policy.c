#include "coop_host_migration_policy.h"

#include <string.h>

coop_host_migration_decision coop_host_migration_decide(
    int current_master,
    int disconnected_player,
    int local_player,
    const int8_t *connection_states,
    int player_count,
    int playing_state)
{
	coop_host_migration_decision result = { COOP_HOST_MIGRATION_NOT_HOST, current_master };
	int player;

	if (disconnected_player != current_master)
		return result;
	result.action = COOP_HOST_MIGRATION_NO_REPLACEMENT;
	result.new_master = -1;
	if (!connection_states || player_count <= 0)
		return result;

	for (player = 0; player < player_count; player++) {
		if (player != disconnected_player && connection_states[player] == playing_state) {
			result.new_master = player;
			result.action = player == local_player ? COOP_HOST_MIGRATION_LOCAL_HOST : COOP_HOST_MIGRATION_REMOTE_HOST;
			break;
		}
	}
	return result;
}

void coop_host_migration_reset_object_owners(int8_t *object_owners, int object_count)
{
	if (object_owners && object_count > 0)
		memset(object_owners, -1, (size_t) object_count);
}
