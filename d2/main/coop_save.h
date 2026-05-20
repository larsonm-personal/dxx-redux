/* Shared coop save declarations live in android/app/src/main/cpp/shared/coop. */
#define COOP_SAVE_METADATA_EXTRA_FIELDS \
	int8_t   escort_owner_player; /* v2: Escort_owner_player (-1 = unowned) */ \
	uint8_t  buddy_allowed_to_talk; /* v2: Buddy_allowed_to_talk */
#include "../../android/app/src/main/cpp/shared/coop/coop_save.h"
#undef COOP_SAVE_METADATA_EXTRA_FIELDS