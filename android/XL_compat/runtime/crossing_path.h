#ifndef DXX_XL_CROSSING_PATH_H
#define DXX_XL_CROSSING_PATH_H
#ifdef __cplusplus
extern "C" {
#endif
struct fvi_query;
struct fvi_info;
/* Repair XL crossing metadata without changing the accepted collision */
void xl_repair_crossing_path(const struct fvi_query *query, struct fvi_info *hit);
#ifdef __cplusplus
}
#endif
#endif
