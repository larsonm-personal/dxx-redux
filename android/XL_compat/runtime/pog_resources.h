#ifndef DXX_XL_POG_RESOURCES_H
#define DXX_XL_POG_RESOURCES_H
#include "physfs.h"
#ifdef __cplusplus
extern "C" {
#endif
struct xl_pog_resources;
/* Prepare only after bounded POG validation; no live bitmap changes */
struct xl_pog_resources *xl_pog_prepare(PHYSFS_file *file);
void xl_pog_free(struct xl_pog_resources *resources);
int xl_pog_install(struct xl_pog_resources *resources);
/* Diagnostic receipt for the live installed payload, not source-file metadata */
int xl_pog_bitmap_receipt(const struct xl_pog_resources *resources, int bitmap, unsigned *bytes, unsigned *fnv1a);
#ifdef __cplusplus
}
#endif
#endif
