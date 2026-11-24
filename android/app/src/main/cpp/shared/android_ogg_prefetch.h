#ifndef DXX_ANDROID_OGG_PREFETCH_H
#define DXX_ANDROID_OGG_PREFETCH_H

/* The game thread owns a single optional decode for the current level load */
void android_ogg_prefetch_start(const char *filename);
void android_ogg_prefetch_clear(void);

#endif
