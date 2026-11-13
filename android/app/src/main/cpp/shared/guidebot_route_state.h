#ifndef GUIDEBOT_ROUTE_STATE_H
#define GUIDEBOT_ROUTE_STATE_H

#include <stdint.h>
#include "../../../../../XL_compat/capacity.h"

/* Checked against the native label/recalc definitions in guidebot_route.c */
#define GUIDEBOT_ROUTE_LABEL_BYTES  64
#define GUIDEBOT_ROUTE_RECALC_SLOTS 4

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct guidebot_route_state {
	int32_t enabled;
	int32_t trigger_capacity;
	int32_t restore_pending;
	int64_t cache_poll_wait;
#define ROUTE_FIELD(type, name, source, kind)            type name;
#define ROUTE_LIMIT(type, name, source, kind, low, high) ROUTE_FIELD(type, name, source, kind)
#define ROUTE_HOST_CLOCK(type, name, source, kind)       ROUTE_FIELD(type, name, source, kind)
#define ROUTE_ARRAY(type, name, source, capacity, kind)  type name[capacity];
#define ROUTE_TEXT(name, source, capacity)               char name[capacity];
#define ROUTE_TRIGGER_BYTES(name, source)                uint8_t name[DXX_XL_MAX_TRIGGERS];
#include "guidebot_route_state_members.h"
#undef ROUTE_FIELD
#undef ROUTE_LIMIT
#undef ROUTE_HOST_CLOCK
#undef ROUTE_ARRAY
#undef ROUTE_TEXT
#undef ROUTE_TRIGGER_BYTES
} guidebot_route_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* Copies owner state and its bounded poll wait using the existing cached clock */
void escort_route_capture_runtime_state(guidebot_route_state *state);
#ifdef __cplusplus
}
#endif
#endif
