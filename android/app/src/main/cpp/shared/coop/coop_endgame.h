#ifndef COOP_ENDGAME_H
#define COOP_ENDGAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Final completion is shared; presentation and result dismissal are local */
void coop_endgame_begin(void);
void coop_endgame_reset(void);
int coop_endgame_active(void);
int coop_endgame_released(void);
int coop_endgame_host_disconnected(int player);
void coop_endgame_receive(const unsigned char *packet, int sender);
void coop_endgame_network_frame(void);
/* Called by the event loop, including movie, briefing and modal windows */
void coop_endgame_pump(void);

#ifdef __cplusplus
}
#endif
#endif
