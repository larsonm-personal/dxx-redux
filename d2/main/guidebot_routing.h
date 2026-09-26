#ifndef D2_GUIDEBOT_ROUTING_H
#define D2_GUIDEBOT_ROUTING_H

#include "vecmat.h"

/* Keep values synchronized with GuidebotRoutingMode.kt and saved/replay metadata */
enum guidebot_routing_mode {
	GUIDEBOT_ROUTING_ORIGINAL = 0,
	GUIDEBOT_ROUTING_ENHANCED = 1
};

int guidebot_routing_valid(int mode);
int guidebot_routing_mode(void);
int guidebot_routing_is_enhanced(void);
const char *guidebot_routing_name(void);
int guidebot_routing_default(void);
/* Launcher preference only; session changes belong on the game thread */
void guidebot_routing_set_default(int mode);
void guidebot_routing_start_session(void);
void guidebot_routing_set_mode(int mode);
/* Restore without discarding the AI path/timers just read from the save */
void guidebot_routing_restore_mode(int mode);
void guidebot_original_path_smoothing(vms_vector *velocity, const vms_vector *goal, fix frame_time);
void escort_reset_routing(void);

#endif
