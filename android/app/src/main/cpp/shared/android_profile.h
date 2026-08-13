#ifndef ANDROID_PROFILE_H
#define ANDROID_PROFILE_H

#if defined(ANDROID) || defined(__ANDROID__)

enum android_profile_bucket {
	ANDROID_PROFILE_BUCKET_WAIT = 0,
	ANDROID_PROFILE_BUCKET_SIM,
	ANDROID_PROFILE_BUCKET_RENDER,
	ANDROID_PROFILE_BUCKET_REPLAY,
	ANDROID_PROFILE_BUCKET_COUNT
};

enum android_profile_texture_lookup_slot {
	ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_SET = 0,
	ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_PREFIX,
	ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_BASE,
	ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT
};

enum android_profile_texture_lookup_ext {
	ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_PNG = 0,
	ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_JPG,
	ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_TGA,
	ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT
};

#define ANDROID_PROFILE_TEXTURE_LOOKUP_NONE (-1)

struct android_profile_texture_lookup_metrics {
	int ktx2_hit_slot;
	int png_hit_slot;
	int png_hit_ext;
	unsigned int ktx2_attempts;
	unsigned int png_attempts;
	long long ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT];
	long long png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT];
	long long png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT];
};

struct android_profile_level_load_metrics {
	long long total_us;
	long long file_us;
	long long presentation_us;
	long long endlevel_us;
	long long replacements_us;
	long long robots_us;
	long long textures_us;
	long long network_us;
	long long sound_us;
	long long music_us;
	long long finish_us;
};

struct android_profile_level_init_metrics {
	long long total_us;
	long long pre_load_us;
	long long load_us;
	long long network_sync_us;
	long long post_load_us;
};

struct android_profile_restore_metrics {
	long long total_us;
	long long pre_level_us;
	long long level_init_us;
	long long state_data_us;
	long long finalize_us;
};

void android_profile_texture_lookup_note_ktx2(
    struct android_profile_texture_lookup_metrics *lookup, int slot,
    long long elapsed_us, int loaded);
long long android_profile_monotonic_us(void);
long long android_profile_take_elapsed_us(long long *last_us);
void android_profile_log_level_load(
    const char *game, int level, const char *file, int game_mode,
    const struct android_profile_level_load_metrics *metrics);
void android_profile_log_level_init(
    const char *game, int requested_level, int current_level, int game_mode,
    const struct android_profile_level_init_metrics *metrics);
void android_profile_log_restore(
    const char *game, int level, const char *file, int game_mode,
    int had_game_window, const struct android_profile_restore_metrics *metrics);
void android_profile_frame_begin(const char *game, unsigned int frame_id);
void android_profile_set_frame_context(int level, int viewer_segment);
void android_profile_set_frame_pacing(int max_fps, int vsync);
void android_profile_set_simulation_metrics(unsigned int simulation_frame_id,
                                            int frame_time_us);
void android_profile_note_flip(void);
long long android_profile_network_begin(void);
void android_profile_network_packet(int packet_bytes);
void android_profile_network_end(long long start_us);
void android_profile_remote_robot_update(int objnum, int signature);
void android_profile_remote_robot_live(int objnum, int signature,
                                       int remote_owned);
void android_profile_set_scene_object_counts(int active_objects,
                                             int projectile_objects,
                                             int reactor_objects);
void android_profile_resume(void);
void android_profile_set_slowdown_capture_enabled(int enabled);
void android_profile_bucket_begin(int bucket);
void android_profile_bucket_end(int bucket);
long long android_profile_object_begin(void);
void android_profile_object_end(long long start_us, int objnum, int object_type,
                                int object_id, int render_type, int model_num);
void android_profile_set_gl_frame_metrics(int swap_us, int gpu_us,
                                          int resolve_us, int gl_error_us);
void android_profile_texture_load(const char *game, const char *name,
                                  const char *source, int width, int height,
                                  int flags, long long total_us,
                                  long long ktx2_read_us,
                                  long long png_read_us,
                                  long long upload_us,
                                  long long mask_us,
                                  const struct android_profile_texture_lookup_metrics *lookup);
void android_profile_storage_op(const char *name, const char *op,
                                unsigned long long offset,
                                unsigned long long size,
                                long long total_us);
void android_profile_frame_end(void);
void android_profile_flush(void);

#else

#define android_profile_frame_begin(game, frame_id)                                                                                             ((void) 0)
#define android_profile_set_frame_context(level, viewer_segment)                                                                                ((void) 0)
#define android_profile_set_frame_pacing(max_fps, vsync)                                                                                        ((void) 0)
#define android_profile_set_simulation_metrics(simulation_frame_id, frame_time_us)                                                              ((void) 0)
#define android_profile_note_flip()                                                                                                             ((void) 0)
#define android_profile_network_begin()                                                                                                         0LL
#define android_profile_network_packet(packet_bytes)                                                                                            ((void) 0)
#define android_profile_network_end(start_us)                                                                                                   ((void) 0)
#define android_profile_remote_robot_update(objnum, signature)                                                                                  ((void) 0)
#define android_profile_remote_robot_live(objnum, signature, remote_owned)                                                                      ((void) 0)
#define android_profile_set_scene_object_counts(active_objects, projectile_objects, reactor_objects)                                            ((void) 0)
#define android_profile_resume()                                                                                                                ((void) 0)
#define android_profile_set_slowdown_capture_enabled(enabled)                                                                                   ((void) 0)
#define android_profile_bucket_begin(bucket)                                                                                                    ((void) 0)
#define android_profile_bucket_end(bucket)                                                                                                      ((void) 0)
#define android_profile_object_begin()                                                                                                          0LL
#define android_profile_object_end(start_us, objnum, object_type, object_id, render_type, model_num)                                            ((void) 0)
#define android_profile_set_gl_frame_metrics(swap_us, gpu_us, resolve_us, gl_error_us)                                                          ((void) 0)
#define android_profile_texture_load(game, name, source, width, height, flags, total_us, ktx2_read_us, png_read_us, upload_us, mask_us, lookup) ((void) 0)
#define android_profile_storage_op(name, op, offset, size, total_us)                                                                            ((void) 0)
#define android_profile_frame_end()                                                                                                             ((void) 0)
#define android_profile_flush()                                                                                                                 ((void) 0)

#endif

#endif /* ANDROID_PROFILE_H */
