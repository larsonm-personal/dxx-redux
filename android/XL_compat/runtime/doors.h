#ifndef DXX_XL_DOORS_H
#define DXX_XL_DOORS_H
#ifdef __cplusplus
extern "C" {
#endif
struct active_door;
int xl_door_open(int segment, int side);
int xl_door_close(int segment, int side, int force);
int xl_door_frame_process(void);
int xl_door_state_valid(const struct active_door *doors, int count);
#ifdef __cplusplus
}
#endif
#endif
