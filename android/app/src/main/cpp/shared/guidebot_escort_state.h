#ifndef GUIDEBOT_ESCORT_STATE_H
#define GUIDEBOT_ESCORT_STATE_H

#include <stdint.h>
#include "guidebot_goal_message.h"

/* ai.h MAX_STOLEN_ITEMS, checked against the native owner at compile time */
#define GUIDEBOT_ESCORT_STOLEN_SLOTS 10

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct guidebot_escort_state {
#define ESCORT_FIELD(type, name, kind)            type name;
#define ESCORT_LIMIT(type, name, kind, low, high) ESCORT_FIELD(type, name, kind)
#define ESCORT_TEXT(name, capacity)               char name[capacity];
#include "guidebot_escort_state_members.h"
#undef ESCORT_FIELD
#undef ESCORT_LIMIT
#undef ESCORT_TEXT

	uint8_t stolen[GUIDEBOT_ESCORT_STOLEN_SLOTS];
	int32_t persistent_message;
} guidebot_escort_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* Pure game-thread observation; no save codec, cache polling or owner repair */
void escort_capture_runtime_state(guidebot_escort_state *state);
#ifdef __cplusplus
}
#endif
#endif
