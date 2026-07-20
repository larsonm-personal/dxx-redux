#ifndef _ANDROID_LOADING_PROGRESS_H
#define _ANDROID_LOADING_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

void android_loading_progress_begin(const char *phase_label, int total_items);
void android_loading_progress_step(const char *item_label);
void android_loading_progress_update(const char *item_label, int completed, int total_items);
void android_loading_progress_end(void);
int android_loading_progress_get_flush_count(void);

#ifdef __cplusplus
}
#endif

#endif
