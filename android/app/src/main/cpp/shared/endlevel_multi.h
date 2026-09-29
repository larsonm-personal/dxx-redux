#ifndef DXX_ENDLEVEL_MULTI_H
#define DXX_ENDLEVEL_MULTI_H

#include <stdint.h>
#include "object.h"

/* UINT32_MAX means no normal exit observed in this world */
#ifdef __cplusplus
extern "C" {
#endif
void endlevel_multi_reset(void);
void endlevel_multi_note_exit(int player, uint32_t age_ms);
uint32_t endlevel_multi_exit_age(int player);
void endlevel_multi_begin(void);
void endlevel_multi_end(void);
void endlevel_multi_frame(void);
void endlevel_multi_render(void);
void endlevel_multi_render_names(void);
int endlevel_multi_local_finished(void);

typedef struct endlevel_multi_actor_state {
	int active, outside, finished, visible, label_visible;
	uint32_t age_ms;
	int segment;
	vms_vector position;
} endlevel_multi_actor_state;
void endlevel_multi_get_actor(int player, endlevel_multi_actor_state *state);
#ifdef __cplusplus
}
#endif
#endif
