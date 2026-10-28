#ifndef DXX_XL_FLUIDS_H
#define DXX_XL_FLUIDS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xl_fluid_clear(void);
int xl_lava_segment(int segment);
int xl_fluid_wall(int object, int segment, int side);
int xl_fluid_frame(int object, int *playing);
int xl_fluid_scrape(int object, int segment, int side, const int32_t point[3]);
int64_t xl_fluid_scrape_elapsed(void);
int xl_fluid_restore(int64_t elapsed, int apply);
#ifdef __cplusplus
}
#endif
#endif
