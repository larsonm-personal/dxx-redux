#ifndef COOP_HOST_MIGRATION_POLICY_H
#define COOP_HOST_MIGRATION_POLICY_H

#include <stdint.h>

typedef enum coop_host_migration_action {
	COOP_HOST_MIGRATION_NOT_HOST = 0,
	COOP_HOST_MIGRATION_NO_REPLACEMENT = 1,
	COOP_HOST_MIGRATION_REMOTE_HOST = 2,
	COOP_HOST_MIGRATION_LOCAL_HOST = 3,
} coop_host_migration_action;

typedef struct coop_host_migration_decision {
	coop_host_migration_action action;
	int new_master;
} coop_host_migration_decision;

coop_host_migration_decision coop_host_migration_decide(
    int current_master,
    int disconnected_player,
    int local_player,
    const int8_t *connection_states,
    int player_count,
    int playing_state);

void coop_host_migration_reset_object_owners(int8_t *object_owners, int object_count);

#endif /* COOP_HOST_MIGRATION_POLICY_H */
