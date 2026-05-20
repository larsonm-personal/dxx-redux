#ifndef STATE_ANDROID_SHARED_H
#define STATE_ANDROID_SHARED_H

/* Shared Android state helpers. Implementation lives in state_android_shared.c. */

extern int g_android_save_blank_thumbnail;

rewind_file *state_android_open_read_buffered(const char *filename);
rewind_file *state_android_open_write_buffered(const char *filename);
int state_android_close_file(rewind_file *file);
void state_android_write_save_metadata(rewind_file *fp, const char *desc,
                                       const char *mission_filename);
void state_android_restore_player_flight_state(void);
int state_android_save_to_path(const char *filename, const char *desc,
                               int save_kind, int blank_thumbnail);
int state_save_to_memory(rewind_memory_buffer *buffer, const char *desc,
                         int save_kind, int blank_thumbnail);
int state_restore_from_memory(const rewind_memory_buffer *buffer);
int state_android_save_to_slot(int slotnum, const char *desc, int save_kind);

#endif