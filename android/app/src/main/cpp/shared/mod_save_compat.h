#ifndef MOD_SAVE_COMPAT_H
#define MOD_SAVE_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rewind_file.h"

/* Capture immutable definitions after loading asset tables, before animation */
void mod_save_capture_definitions(void);
void mod_save_capture_level(const char *filename);
void mod_save_capture_extra_ham(const char *filename);
void mod_save_effect_offset(rewind_file *file);
int mod_save_write(rewind_file *file);
/* Called after mission selection but before replacing the saved world */
int mod_save_prepare(rewind_file *file, int level, int version, int swap);
int mod_save_check_companion(const char *filename);
void mod_save_skip_trailer(rewind_file *file);
int mod_save_validate_effects(rewind_file *file, int swap);
void mod_save_read_effects(rewind_file *file, int swap, int apply);
int mod_save_adapt_world(void);
void mod_save_begin(void);
void mod_save_finished(int success);
void mod_save_show_error(void);
const char *mod_save_status(void);
const char *mod_save_reason(void);
int mod_save_adapted_count(void);

#ifdef __cplusplus
}
#endif
#endif
