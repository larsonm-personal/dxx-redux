#ifndef DXX_XL_CLOAK_H
#define DXX_XL_CLOAK_H
#ifdef __cplusplus
extern "C" {
#endif
struct cloaking_wall;
/* Zero selects Redux's alternate; one means XL handled the operation */
int xl_start_wall_cloak(int segment, int side, int opening);
int xl_cloak_frame_process(void);
int xl_cloak_saved_valid(const struct cloaking_wall *entries, int count);
#ifdef __cplusplus
}
#endif
#endif
