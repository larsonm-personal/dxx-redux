/* Sparse Android startup diagnostics; logcat supplies device-local timestamps */
#ifndef STARTUP_SYNC_TRACE_H
#define STARTUP_SYNC_TRACE_H
#ifdef __ANDROID__
#define STARTUP_SYNC_TRACE(event)                                                      \
	debug_log_force(DLOG_NETWORK,                                                      \
	                "[STARTUP-SYNC] event=%s level=%d status=%d player=%d players=%d", \
	                event, Current_level_num, Network_status, Player_num, N_players)
#else
#define STARTUP_SYNC_TRACE(event) ((void) 0)
#endif
#endif
