#ifndef COOP_HOST_MIGRATION_H
#define COOP_HOST_MIGRATION_H

/* Returns nonzero when a cooperative host loss was migrated in place. */
int coop_host_migration_handle_disconnect(int disconnected_player);

#endif /* COOP_HOST_MIGRATION_H */
