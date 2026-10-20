/* Debug-only deterministic wire faults for the public LAN recovery owner.
 * Captured datagrams are replayed byte-for-byte after signing, with bounded
 * storage and no changes to production authentication or retry policy */
#ifndef DXX_NET_UDP_STARTUP_FAULTS_H
#define DXX_NET_UDP_STARTUP_FAULTS_H
#if defined(__ANDROID__) && !defined(NDEBUG)
static struct {
	int captured, dropped, sockfd, len;
	unsigned flags;
	fix64 due;
	struct sockaddr_storage address;
	socklen_t address_len;
	unsigned char bytes[4096];
} startup_fault;

static unsigned startup_fault_hash(const void *data, int len)
{
	const unsigned char *bytes = data;
	unsigned hash = 2166136261u;
	for (int i = 0; i < len; ++i) hash = (hash ^ bytes[i]) * 16777619u;
	return hash;
}

static void startup_fault_pump(void)
{
	if (!startup_fault.len || timer_query() < startup_fault.due) return;
	const int len = startup_fault.len;
	startup_fault.len = 0;
	const int sent = sendto(startup_fault.sockfd, startup_fault.bytes, len, startup_fault.flags,
	                        (struct sockaddr *) &startup_fault.address, startup_fault.address_len);
	debug_log_force(DLOG_NETWORK, "[STARTUP-FAULT] release type=%u bytes=%d hash=%08x sent=%d status=%d",
	                startup_fault.bytes[0], len, startup_fault_hash(startup_fault.bytes, len), sent, Network_status);
}

/* mode: 1 replay admission; 2 retag admission as REQUEST; 3 drop two requests;
 * 4 drop one proof; 5 delay one proof; 6 replay one proof; 7 delay admission */
static int startup_fault_send(int sockfd, const void *data, int len, unsigned flags,
                              const struct sockaddr *address, socklen_t address_len)
{
	const int mode = auto_net_startup_test_packet_fault;
	if (!mode || len <= 0) return 0;
	const unsigned type = ((const unsigned char *) data)[0];
	if (((mode == 3 && type == UPID_REQUEST && startup_fault.dropped < 2) ||
	     (mode == 4 && type == UPID_RECONNECT_PROOF && !startup_fault.dropped))) {
		++startup_fault.dropped;
		debug_log_force(DLOG_NETWORK, "[STARTUP-FAULT] drop type=%u bytes=%d hash=%08x", type, len, startup_fault_hash(data, len));
		return 1;
	}
	if (startup_fault.captured || len > (int) sizeof(startup_fault.bytes) ||
	    address_len > sizeof(startup_fault.address)) return 0;
	if (!(((mode == 1 || mode == 2 || mode == 7) && type == UPID_LAUNCH_ADMISSION) ||
	      ((mode == 5 || mode == 6) && type == UPID_RECONNECT_PROOF))) return 0;
	startup_fault.captured = 1;
	startup_fault.sockfd = sockfd;
	startup_fault.len = len;
	startup_fault.flags = flags;
	startup_fault.address_len = address_len;
	memcpy(&startup_fault.address, address, address_len);
	memcpy(startup_fault.bytes, data, len);
	startup_fault.due = timer_query() + 6 * F1_0;
	if (mode == 2) startup_fault.bytes[0] = UPID_REQUEST;
	debug_log_force(DLOG_NETWORK, "[STARTUP-FAULT] capture mode=%d type=%u bytes=%d hash=%08x status=%d",
	                mode, startup_fault.bytes[0], len, startup_fault_hash(startup_fault.bytes, len), Network_status);
	return mode == 5 || mode == 7;
}
#endif
#endif
