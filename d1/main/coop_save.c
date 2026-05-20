/* Shared coop save implementation lives in android/app/src/main/cpp/shared/coop. */
#define COOP_SAVE_RESTORE_FLAGS_DURABLE ( \
	PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_MAP_ALL)
#include "../../android/app/src/main/cpp/shared/coop/coop_save.c"
#undef COOP_SAVE_RESTORE_FLAGS_DURABLE