#ifndef DXX_GRAPHICS_SAFETY_STORE_H
#define DXX_GRAPHICS_SAFETY_STORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Field order also defines the JNI snapshot contract in NativeGraphicsSafety.kt */
enum graphics_safety_field {
	GRAPHICS_SAFE_TEXFILT,
	GRAPHICS_SAFE_ANISO,
	GRAPHICS_SAFE_MSAA,
	GRAPHICS_SAFE_MENU_FILTER,
	GRAPHICS_SAFE_HUD_FILTER,
	GRAPHICS_SAFE_WIDTH,
	GRAPHICS_SAFE_HEIGHT,
	GRAPHICS_SAFE_ASPECT_X,
	GRAPHICS_SAFE_ASPECT_Y,
	GRAPHICS_SAFE_COLOR_DEPTH,
	GRAPHICS_SAFE_FIELD_COUNT
};

struct graphics_safety_snapshot {
	int values[GRAPHICS_SAFE_FIELD_COUNT];
};

enum graphics_safety_phase {
	GRAPHICS_SAFE_IDLE,
	GRAPHICS_SAFE_ATTEMPT,
	GRAPHICS_SAFE_PREPARING,
	GRAPHICS_SAFE_CHALLENGE,
	GRAPHICS_SAFE_RESTORING
};

struct graphics_safety_record {
	struct graphics_safety_snapshot accepted;
	struct graphics_safety_snapshot candidate;
	uint64_t trial_id;
	uint64_t deadline_ms;
	int owner_pid;
	uint64_t owner_session;
	int phase;
	char reason[96];
	unsigned pending_mask;
	struct graphics_safety_snapshot pending;
};

extern const char *const graphics_safety_keys[GRAPHICS_SAFE_FIELD_COUNT];
void graphics_safety_defaults(struct graphics_safety_snapshot *snapshot);
int graphics_safety_normalize(struct graphics_safety_snapshot *snapshot);
int graphics_safety_equal(const struct graphics_safety_snapshot *a,
                          const struct graphics_safety_snapshot *b);
int graphics_safety_all_off(const struct graphics_safety_snapshot *snapshot);
int graphics_safety_field_for_key(const char *key);

/* Lock protects record and config files across launcher/game processes
 * Reentrant within one native thread; never hold it across GL or Java callbacks */
void *graphics_safety_lock(const char *root);
void graphics_safety_unlock(void *lock);
int graphics_safety_read_requested(const char *root, const char *game,
                                   struct graphics_safety_snapshot *snapshot);
int graphics_safety_read_staged(const char *root, const char *game,
                                struct graphics_safety_snapshot *snapshot);
struct graphics_config_update;
/* 1 published, 2 durably deferred until the active trial resolves, 0 failure */
int graphics_safety_stage(const char *root, const struct graphics_config_update *updates, size_t count);
/* 2 deferred edits published, 1 nothing to publish or trial still active, 0 failure */
int graphics_safety_flush_staged(const char *root);
int graphics_safety_read_record(const char *root, struct graphics_safety_record *record);
int graphics_safety_begin_attempt(const char *root, const struct graphics_safety_snapshot *candidate,
                                  uint64_t trial_id, int owner_pid, int preparing);
int graphics_safety_arm(const char *root, uint64_t trial_id, uint64_t now_ms);
/* 1 accepted, 2 restore required, 0 stale, -1 storage failure */
int graphics_safety_decide(const char *root, uint64_t trial_id, int accept,
                           uint64_t now_ms, const char *reason);
int graphics_safety_patch_snapshot(const char *root, const struct graphics_safety_snapshot *snapshot);
int graphics_safety_complete_restore(const char *root, uint64_t trial_id);
/* 2 abandoned attempt repaired, 1 no repair required, 0 storage failure */
int graphics_safety_recover(const char *root, int current_pid);
int graphics_safety_clean_exit(const char *root, int owner_pid);

#ifdef __cplusplus
}
#endif
#endif
