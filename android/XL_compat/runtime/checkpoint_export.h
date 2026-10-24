#ifndef XL_CHECKPOINT_EXPORT_H
#define XL_CHECKPOINT_EXPORT_H
struct PHYSFS_File;
#ifdef __cplusplus
extern "C" {
#endif
void xl_checkpoint_capture_sources(const char *level);
void xl_checkpoint_effect_offset(struct PHYSFS_File *file);
int xl_checkpoint_finish_export(const char *filename);
#ifdef __cplusplus
}
#endif
#endif
