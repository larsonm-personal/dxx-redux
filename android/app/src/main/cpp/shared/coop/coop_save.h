/*
 * Coop save-file metadata extension.
 *
 * Appended as a tagged trailer after the existing save data so old
 * builds skip it transparently. New builds detect the tag, read the
 * metadata, and use it for player-identity matching on restore.
 *
 * Shared between d1 and d2. Fixed-size weapon arrays use the d2
 * maximum (10) so the binary layout is identical for both games;
 * unused slots are zeroed for d1 which has 5 weapons.
 *
 * Duplicated constant: COOP_CLIENT_ID_LEN = 36 (also in ClientIdentity.kt)
 */

#ifndef COOP_SAVE_H
#define COOP_SAVE_H

#include "pstypes.h"
#include "fix.h"

#ifndef COOP_SAVE_METADATA_EXTRA_FIELDS
#define COOP_SAVE_METADATA_EXTRA_FIELDS
#endif

/* --- constants --- */
#define COOP_SAVE_META_TAG          0x434F4F50 /* "COOP" */
#define COOP_SAVE_META_VER          3
#define COOP_MAX_REMEMBERED_PLAYERS 16
#define COOP_CLIENT_ID_LEN          36         /* UUID without null */
#define COOP_SAVE_MAX_WEAPONS       10         /* max(d1=5, d2=10) */
#define COOP_CALLSIGN_LEN           8          /* matches CALLSIGN_LEN */
#define COOP_AUTOSAVE_SLOT          9          /* legacy single slot */
#define COOP_AUTOSAVE_SLOT_FIRST    5          /* first rotating autosave slot */
#define COOP_AUTOSAVE_SLOT_COUNT    5          /* slots 5,6,7,8,9 */
#define COOP_AUTOSAVE_GAME_ID       0x434F4F50 /* sentinel state_game_id for autosaves */
#define COOP_AUTOSAVE_CALLSIGN      "coopsave" /* stable 8-char filename prefix */

/* --- per-player record stored in the trailer --- */
typedef struct coop_player_record {
	char callsign[COOP_CALLSIGN_LEN + 1];
	char client_id[COOP_CLIENT_ID_LEN + 1]; /* UUID string or empty */
	int32_t score;
	uint8_t was_connected; /* 1 = playing when saved, 0 = absent/carried */
	fix energy;
	fix shields;
	uint8_t laser_level;
	uint16_t primary_weapon_flags;
	uint16_t secondary_weapon_flags;
	uint16_t primary_ammo[COOP_SAVE_MAX_WEAPONS];
	uint16_t secondary_ammo[COOP_SAVE_MAX_WEAPONS];
	uint32_t flags; /* player flags (keys, powerups) */
	/* v3 stats fields */
	int16_t net_kills_total;
	int16_t net_killed_total;
	int16_t num_kills_total;
	uint16_t hostages_rescued_total;
	fix time_total;
	int8_t hours_total;
	uint8_t original_slot; /* player slot index at save time */
} coop_player_record;

/* --- trailer appended after existing coop save data --- */
typedef struct coop_save_metadata {
	uint32_t tag;                  /* COOP_SAVE_META_TAG */
	uint16_t version;              /* COOP_SAVE_META_VER */
	uint32_t wall_clock_timestamp; /* Unix epoch seconds */
	int16_t level_num;
	char mission_name[9];
	uint8_t difficulty;
	uint8_t num_active_players;
	uint8_t num_absent_players;
	COOP_SAVE_METADATA_EXTRA_FIELDS
	coop_player_record active_players[8]; /* MAX_PLAYERS */
	coop_player_record absent_players[COOP_MAX_REMEMBERED_PLAYERS];
} coop_save_metadata;

/* --- helpers (implemented in coop_save.c) --- */

/* Fill a coop_player_record from the live player struct at slot pnum.
 * was_connected is set to 1. */
void coop_snapshot_player(int pnum, coop_player_record *rec);

/* Write the coop metadata trailer to an already-open save file.
 * Call this just before PHYSFS_close(). */
void coop_write_save_metadata(void *fp);

/* Try to read the coop metadata trailer from an open save file.
 * Seeks to the position after the existing data (caller should pass
 * the expected end offset). Returns 1 if metadata was found and
 * parsed, 0 if missing or corrupt. On success, fills *meta. */
int coop_read_save_metadata(void *fp, PHYSFS_sint64 expected_end,
                            coop_save_metadata *meta);

/* Find a player in the metadata by client_id (preferred) or callsign
 * (fallback). Returns:
 *   0..7              index into active_players[]
 *   8..8+MAX_ABSENT   offset into absent_players[] (subtract 8)
 *   -1                not found (new player) */
int coop_find_player_in_metadata(const char *callsign,
                                 const char *client_id,
                                 const coop_save_metadata *meta);

/* --- absent player tracking (in-memory, survives level transitions) --- */

/* Snapshot a disconnecting player into the absent list.
 * Called from multi_disconnect_player() in coop mode. */
void coop_track_absent_player(int pnum);

/* Clear all absent player records (e.g. at game start). */
void coop_clear_absent_players(void);

/* Get count and pointer to the absent player array (read-only). */
int coop_get_num_absent_players(void);
const coop_player_record *coop_get_absent_players(void);

/* Find an absent player by client_id (preferred) or callsign (fallback).
 * Returns pointer to the record, or NULL if not found. */
const coop_player_record *coop_find_absent_player(const char *callsign,
                                                  const char *client_id);

/* Repopulate the absent player list from save metadata.
 * Called after state_restore_all_sub() loads a coop save. */
void coop_load_absent_from_metadata(const coop_save_metadata *meta);

/* --- auto-save (Phase 3) --- */

/* Trigger a coop auto-save to slot COOP_AUTOSAVE_SLOT.
 * Returns 1 on success, 0 if conditions prevent saving. */
int coop_autosave(void);

/* --- inventory application helper --- */

/* Apply a saved player record to a live player slot.
 * If same_level is true, key flags (blue/red/gold) are also restored.
 * Updates ConsoleObject shields when pnum == Player_num. */
void coop_apply_record_to_player(int pnum, const coop_player_record *rec,
                                 int same_level);

/* --- progress tracking (Phase 4) --- */

/* Write coop_progress.json recording the last completed level.
 * Called at the end of each coop level (DoEndLevelScoreGlitz).
 * Also writes coop_progress_inventory.bin as a binary sidecar. */
void coop_write_progress_json(void);

/* Load progress inventory from coop_progress_inventory.bin.
 * Populates the absent list for other players; directly restores the
 * host's own inventory. Returns 1 if inventory was loaded, 0 otherwise.
 * Only loads if the mission matches and level_num == Current_level_num - 1.
 * Called once at coop game start (one-shot gate). */
int coop_load_progress_inventory(void);

/* --- auto-restore (Phase 4) --- */

/* Check for a viable auto-save for the current mission and arm the
 * auto-restore if found. Called once when a coop game starts. */
void coop_arm_auto_restore(void);

/* Try to trigger auto-restore. Called each frame from multi_do_frame.
 * Fires once when all players are connected, then disarms itself. */
void coop_try_auto_restore(void);

/* Disarm auto-restore (e.g. if the player doesn't want it). */
void coop_disarm_auto_restore(void);

#endif /* COOP_SAVE_H */