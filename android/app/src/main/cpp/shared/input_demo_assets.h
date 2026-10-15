/* Bind demo fingerprints to the sources actually opened by the engine */
#ifndef INPUT_DEMO_ASSETS_H
#define INPUT_DEMO_ASSETS_H
#include <physfs.h>
#ifdef __cplusplus
extern "C" {
#endif
void input_demo_assets_note_file(const char *role, const char *path, PHYSFS_file *file);
void input_demo_assets_note_archive(const char *role, const char *path);
void input_demo_assets_forget(const char *role);
void input_demo_assets_extra_ham_loaded(void);
int input_demo_assets_mount(const char *role, const char *path, int append);
const char *input_demo_assets_capture(void);
int input_demo_assets_check_replay(char *error, unsigned int capacity);
#ifdef __cplusplus
}
#endif
#endif
