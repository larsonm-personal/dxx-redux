#ifndef XL_TRIGGER_STATE_H
#define XL_TRIGGER_STATE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "rewind_file.h"
#ifdef _MSC_VER
// Engine headers select byte packing; keep this ABI independent of include order
#pragma pack(push, 8)
#endif
typedef struct xl_trigger_state {
	int64_t elapsed;
	int32_t delay_ms, object, shot, operated, destination;
} xl_trigger_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
int xl_wall_trigger_state(int index, xl_trigger_state *state);
int xl_wall_trigger_restore(const xl_trigger_state *states, int count);
int xl_wall_trigger_restore_mode(const xl_trigger_state *states, int count, int apply);
int xl_wall_trigger_write(rewind_file *file);
int xl_wall_trigger_read(rewind_file *file);
int xl_level_trigger_state_required(void);
uint64_t xl_level_trigger_identity(void);
#ifdef __cplusplus
}
#endif
#endif
