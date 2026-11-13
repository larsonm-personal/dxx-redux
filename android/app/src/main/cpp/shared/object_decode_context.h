#ifndef DXX_OBJECT_DECODE_CONTEXT_H
#define DXX_OBJECT_DECODE_CONTEXT_H

#include <stdint.h>
#include "rewind_file.h"

struct object;
struct object_rw;
struct player;
struct player_rw;
struct robot_info;
struct reactor;

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
/* Borrowed candidate definitions and clock, independent of the live world
 * Storage remains owned by the level/save preparation until decoding ends */
typedef struct d2_object_decode_context {
	int64_t epoch;
	int original_d1, xl;
	const struct robot_info *robots;
	int robot_count;
	const struct reactor *reactors;
	int reactor_capacity;
} d2_object_decode_context;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* Native signed32 time offsets must fit before any decoded field is written */
static inline int object_decode_epoch_valid(int64_t epoch)
{
	return epoch >= INT64_MIN + (int64_t) INT32_MAX + 1 && epoch <= INT64_MAX - INT32_MAX;
}

static inline int object_decode_context_valid(const d2_object_decode_context *context)
{
	return context && object_decode_epoch_valid(context->epoch) &&
	       (context->original_d1 == 0 || context->original_d1 == 1) &&
	       (context->xl == 0 || context->xl == 1) &&
	       context->robot_count >= 0 && (!context->robot_count || context->robots) &&
	       (!context->original_d1 || context->robot_count > 0) &&
	       context->reactor_capacity >= 0 && (!context->reactor_capacity || context->reactors);
}

#ifdef __cplusplus
extern "C" {
#endif
int state_decode_object(const struct object_rw *saved, struct object *decoded,
                        int native_ai_format, const d2_object_decode_context *context);
int object_rw_swap_for_context(struct object_rw *saved, int swap, int native_ai_format,
                               const d2_object_decode_context *context);
int state_decode_player(const struct player_rw *saved, struct player *decoded, int64_t epoch);
/* Checked native I/O through the same codecs; failures preserve output and cursor */
int state_read_object(rewind_file *file, int swap, int native_ai_format,
                      const d2_object_decode_context *context, struct object *decoded);
int state_read_player(rewind_file *file, int swap, int64_t epoch, struct player *decoded);
#ifdef __cplusplus
}
#endif
#endif
