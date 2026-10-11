/* Android D1 semantic mod patches, also used with native D1 assets in D2 */
#ifndef ANDROID_D1_MOD_PATCHES_H
#define ANDROID_D1_MOD_PATCHES_H
#ifdef __cplusplus
extern "C" {
#endif
/* Return applied writes, zero when absent, or -1 on rejection without mutation */
int android_d1_mod_apply_properties(void);
int android_d1_mod_apply_level(const char *filename);
#ifdef __cplusplus
}
#endif
#endif
