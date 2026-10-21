/* Validate the wire-fault fixture independently of engine state */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __ANDROID__ 1
#ifdef NDEBUG
#undef NDEBUG
#endif
#define F1_0                  65536
#define DLOG_NETWORK          0
#define UPID_REQUEST          8
#define UPID_RECONNECT_PROOF  32
#define UPID_LAUNCH_ADMISSION 36
typedef int64_t fix64;
typedef unsigned socklen_t;
struct sockaddr {
	unsigned char bytes[16];
};
struct sockaddr_storage {
	unsigned char bytes[128];
};
static int auto_net_startup_test_packet_fault, Network_status;
static fix64 now;
static unsigned char delivered[4096];
static int deliveries;
static fix64 timer_query(void)
{
	return now;
}
#define debug_log_force(...) ((void) 0)
static int sendto(int socket, const void *data, int len, unsigned flags, const struct sockaddr *addr, socklen_t addrlen)
{
	(void) socket;
	(void) flags;
	(void) addr;
	(void) addrlen;
	memcpy(delivered, data, len);
	++deliveries;
	return len;
}
#include "net_udp_startup_faults.h"
#define CHECK(x)                                            \
	do {                                                    \
		if (!(x)) {                                         \
			fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
			return 1;                                       \
		}                                                   \
	} while (0)
int main(void)
{
	struct sockaddr addr = { { 0 } };
	for (int mode = 1; mode <= 7; ++mode) {
		memset(&startup_fault, 0, sizeof(startup_fault));
		auto_net_startup_test_packet_fault = mode;
		now = 100 * F1_0;
		deliveries = 0;
		unsigned char wire[] = { 36, 42, 123, 99 };
		if (mode == 3) wire[0] = UPID_REQUEST;
		if (mode >= 4 && mode <= 6) wire[0] = UPID_RECONNECT_PROOF;
		unsigned char expected[sizeof(wire)];
		memcpy(expected, wire, sizeof(wire));
		if (mode == 2) expected[0] = UPID_REQUEST;
		const int withheld = startup_fault_send(1, wire, sizeof(wire), 0, &addr, sizeof(addr));
		CHECK(withheld == (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 7));
		if (mode == 2) {
			CHECK(deliveries == 1 && !memcmp(expected, delivered, sizeof(expected)));
			wire[0] = UPID_REQUEST;
			CHECK(startup_fault_send(1, wire, sizeof(wire), 0, &addr, sizeof(addr)));
			now += 6 * F1_0;
			CHECK(!startup_fault_send(1, wire, sizeof(wire), 0, &addr, sizeof(addr)));
			continue;
		}
		if (mode == 3 || mode == 4) {
			CHECK(startup_fault_send(1, wire, sizeof(wire), 0, &addr, sizeof(addr)) == (mode == 3));
			CHECK(!startup_fault_send(1, wire, sizeof(wire), 0, &addr, sizeof(addr)));
			CHECK(!startup_fault.len);
			continue;
		}
		CHECK(startup_fault_hash(startup_fault.bytes, sizeof(wire)) == startup_fault_hash(expected, sizeof(wire)));
		wire[1] = 0; /* caller may immediately reuse the original send buffer */
		now += 6 * F1_0 - 1;
		startup_fault_pump();
		CHECK(!deliveries);
		++now;
		Network_status = 3; /* delayed delivery occurs after a state boundary */
		startup_fault_pump();
		CHECK(deliveries == 1 && !memcmp(expected, delivered, sizeof(expected)));
		startup_fault_pump();
		CHECK(deliveries == 1);
	}
	puts("PASS: bounded byte-identical replay, retag, loss and delayed delivery fixture");
	return 0;
}
