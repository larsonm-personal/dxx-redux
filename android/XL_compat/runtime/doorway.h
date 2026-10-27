#ifndef DXX_XL_DOORWAY_H
#define DXX_XL_DOORWAY_H
#ifdef __cplusplus
extern "C" {
#endif
/* -1 selects the ordinary wall path; nonnegative values are source doorway flags */
int xl_wall_doorway(int object, int segment, int side);
#ifdef __cplusplus
}
#endif
#endif
