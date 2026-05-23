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

void android_profile_frame_begin(const char *game, unsigned int frame_id);
void android_profile_bucket_begin(int bucket);
void android_profile_bucket_end(int bucket);
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
#define android_profile_bucket_begin(bucket)                                                                                                    ((void) 0)
#define android_profile_bucket_end(bucket)                                                                                                      ((void) 0)
#define android_profile_set_gl_frame_metrics(swap_us, gpu_us, resolve_us, gl_error_us)                                                          ((void) 0)
#define android_profile_texture_load(game, name, source, width, height, flags, total_us, ktx2_read_us, png_read_us, upload_us, mask_us, lookup) ((void) 0)
#define android_profile_storage_op(name, op, offset, size, total_us)                                                                            ((void) 0)
#define android_profile_frame_end()                                                                                                             ((void) 0)
#define android_profile_flush()                                                                                                                 ((void) 0)

#endif

#endif /* ANDROID_PROFILE_H */