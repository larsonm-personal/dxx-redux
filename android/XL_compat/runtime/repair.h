#ifndef DXX_XL_REPAIR_H
#define DXX_XL_REPAIR_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int xl_repair_segment(int segment);
int32_t xl_repair_amount(int segment, int32_t maximum);
int64_t xl_repair_elapsed(void);
int xl_repair_restore(int64_t elapsed, int apply);
void xl_repair_clear(void);
unsigned xl_repair_cues(void);
int xl_repair_map_color(int segment, int fallback);
unsigned xl_repair_map_calls(void);
#ifdef __cplusplus
}
#endif
#endif
