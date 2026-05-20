/* Shared coop save implementation lives in android/app/src/main/cpp/shared/coop. */
#define COOP_SAVE_HAS_ESCORT_STATE 1
#define COOP_SAVE_DECLARE_PKILLED_FLAGS 1
#define COOP_SAVE_METADATA_EXTRA_FIELDS \
	int8_t   escort_owner_player; \
	uint8_t  buddy_allowed_to_talk;
#define COOP_SAVE_RESTORE_FLAGS_DURABLE ( \
	PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_MAP_ALL | \
	PLAYER_FLAGS_AMMO_RACK | PLAYER_FLAGS_CONVERTER | \
	PLAYER_FLAGS_AFTERBURNER | PLAYER_FLAGS_HEADLIGHT)
#define COOP_SAVE_WRITE_METADATA_EXTRA(meta) \
	do { \
		(meta).escort_owner_player = (int8_t)Escort_owner_player; \
		(meta).buddy_allowed_to_talk = Buddy_allowed_to_talk ? 1 : 0; \
	} while (0)
#define COOP_SAVE_AUTO_RESTORE_TIMEOUT_FRAMES 300
#define COOP_SAVE_AUTO_RESTORE_TIMEOUT_BEFORE_ALIVE_CHECK 1
#define COOP_SAVE_AUTO_RESTORE_TRACE(fmt, ...) COOPLOG(fmt, ##__VA_ARGS__)
#define COOP_SAVE_AUTO_RESTORE_LOG_NO_SLOT() \
	COOPLOG("no restore slot file from lobby")
#define COOP_SAVE_AUTO_RESTORE_LOG_SLOT_NOT_VIABLE(slot) \
	COOPLOG("selected slot %d not viable", (slot))
#define COOP_SAVE_AUTO_RESTORE_LOG_ARMED(slot, gid) \
	COOPLOG("auto-restore armed from lobby-selected slot %d (game_id=%u)", (slot), (gid))
#define COOP_SAVE_AUTO_RESTORE_LOG_TRIGGER(slot, gid, frame) \
	COOPLOG("triggering auto-restore from slot %d (game_id=%u) at frame %d", \
		(slot), (gid), (frame))
#define COOP_SAVE_LOG_PLAYER_STATUS() \
	do { \
		int i; \
		for (i = 0; i < N_players; i++) \
			COOPLOG("  P%d '%s' connected=%d PKilled=%d", \
				i, Players[i].callsign, Players[i].connected, PKilledFlags[i]); \
	} while (0)
#include "../../android/app/src/main/cpp/shared/coop/coop_save.c"
#undef COOP_SAVE_LOG_PLAYER_STATUS
#undef COOP_SAVE_AUTO_RESTORE_LOG_TRIGGER
#undef COOP_SAVE_AUTO_RESTORE_LOG_ARMED
#undef COOP_SAVE_AUTO_RESTORE_LOG_SLOT_NOT_VIABLE
#undef COOP_SAVE_AUTO_RESTORE_LOG_NO_SLOT
#undef COOP_SAVE_AUTO_RESTORE_TRACE
#undef COOP_SAVE_AUTO_RESTORE_TIMEOUT_BEFORE_ALIVE_CHECK
#undef COOP_SAVE_AUTO_RESTORE_TIMEOUT_FRAMES
#undef COOP_SAVE_WRITE_METADATA_EXTRA
#undef COOP_SAVE_RESTORE_FLAGS_DURABLE
#undef COOP_SAVE_METADATA_EXTRA_FIELDS
#undef COOP_SAVE_DECLARE_PKILLED_FLAGS
#undef COOP_SAVE_HAS_ESCORT_STATE