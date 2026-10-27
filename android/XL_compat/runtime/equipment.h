#ifndef XL_EQUIPMENT_H
#define XL_EQUIPMENT_H
#include <stdint.h>
#ifdef __cplusplus
namespace xl_compat
{
struct Inventory;
}
void xl_equipment_load(const xl_compat::Inventory &level);
extern "C" {
#endif
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct xl_equipment_state {
	int64_t timer;
	int segment, enabled, pending;
} xl_equipment_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
void xl_equipment_clear(void);
int xl_equipment_count(void);
int xl_equipment_state_get(int index, xl_equipment_state *state);
int xl_equipment_restore(const xl_equipment_state *states, int count, int apply);
int xl_equipment_trigger(int segment);
void xl_equipment_frame(int first_segment, int end_segment);
#ifdef __cplusplus
}
#endif
#endif
