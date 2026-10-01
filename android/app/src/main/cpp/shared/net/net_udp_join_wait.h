#ifndef NET_UDP_JOIN_WAIT_H
#define NET_UDP_JOIN_WAIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Android pre-admission status, independent of a player's world/barrier slot */
enum {
	UPID_JOIN_QUERY = 33,
	UPID_JOIN_STATUS = 34,
	UPID_JOIN_DATA = 35,
	UPID_JOIN_DATA_HEADER = 17,
	UPID_JOIN_QUERY_SIZE = 17,
	UPID_JOIN_STATUS_SIZE = 57
};
typedef enum net_join_phase {
	NET_JOIN_CONTACT,
	NET_JOIN_READY,
	NET_JOIN_PREPARING,
	NET_JOIN_BRIEFING,
	NET_JOIN_ESCAPE,
	NET_JOIN_FLYOUT,
	NET_JOIN_SCORES,
	NET_JOIN_TRAVEL,
	NET_JOIN_RESTORE,
	NET_JOIN_COMPLETE,
	NET_JOIN_PHASE_COUNT
} net_join_phase;

typedef struct net_join_status {
	uint32_t phase;
	int32_t level;
	uint64_t visit, briefing;
	uint32_t remaining_ms, duration_ms, completed, total, host_ready;
} net_join_status;

void net_join_wait_begin(void);
void net_join_wait_end(void);
int net_join_wait_active(void);
int net_join_wait_cancel(uint64_t generation);
const char *net_join_wait_failure(void);
void net_join_wait_receive(const net_join_status *status, unsigned transit_ms);
void net_join_wait_frame(void);
int net_join_wait_prepare(void);
void net_join_wait_sync_begin(void);
int net_join_wait_restart(void);
void net_join_wait_retry(void);
uint64_t net_join_wait_visit(void);
void net_join_wait_interrupt(void);
int net_join_wait_can_request(void);
int net_join_wait_reader_cancelled(void);
int net_join_wait_ui(char *text, size_t size, uint64_t *generation,
                     int *remaining_ms, int *duration_ms);
void net_join_wait_get_state(net_join_status *status, unsigned *presentations);
/* Implemented by each UDP engine; only listens and exchanges join status */
void net_udp_join_wait_poll(void);
/* Android integration diagnostics: hold after the first object packet */
int net_udp_join_wait_test_hold(void);
void net_join_wait_note_objects(void);
unsigned net_join_wait_object_packets(void);
unsigned net_join_wait_retries(void);

#ifdef __cplusplus
}
#endif
#endif
