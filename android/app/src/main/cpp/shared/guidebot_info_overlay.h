/* Android-only passive Guide-Bot diagnostics, owned by the game thread */
#ifndef GUIDEBOT_INFO_OVERLAY_H
#define GUIDEBOT_INFO_OVERLAY_H
#ifdef __ANDROID__
void guidebot_info_set_visible(int visible);
int guidebot_info_visible(void);
void guidebot_info_reset(void);
int guidebot_info_history_count(void);
const char *guidebot_info_status(void);
void guidebot_info_event(const char *text, int error);
void guidebot_info_draw(void);
#endif
#endif
