#ifndef DXX_XL_KEYED_WALL_H
#define DXX_XL_KEYED_WALL_H
#ifdef __cplusplus
extern "C" {
#endif
/* -1 selects the ordinary wall path; nonnegative values are source doorway flags */
int xl_closed_wall_doorway(int object, int segment, int side);
#ifdef __cplusplus
}
#endif
#endif
