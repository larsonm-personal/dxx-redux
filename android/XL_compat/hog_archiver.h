#ifndef DXX_XL_HOG_ARCHIVER_H
#define DXX_XL_HOG_ARCHIVER_H
#ifdef __cplusplus
extern "C" {
#endif
/* Register immediately after PHYSFS_init, before any HOG is mounted */
int xl_register_hog_archiver(void);
#ifdef __cplusplus
}
#endif
#endif
