#ifndef STARTUP_RESUME_SHARED_H
#define STARTUP_RESUME_SHARED_H

int startup_find_cmd_arg(const char *name);
void startup_apply_pilot_arg(void);
int startup_resume_save_from_cmdline(const char *game_name);

#endif
