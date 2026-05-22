#ifndef ANDROID_PROFILE_H
#define ANDROID_PROFILE_H

#if defined(ANDROID) || defined(__ANDROID__)

void android_profile_frame_begin(const char *game, unsigned int frame_id);
void android_profile_frame_end(void);
void android_profile_flush(void);

#else

#define android_profile_frame_begin(game, frame_id) ((void)0)
#define android_profile_frame_end() ((void)0)
#define android_profile_flush() ((void)0)

#endif

#endif /* ANDROID_PROFILE_H */