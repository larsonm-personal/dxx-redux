#ifndef NET_UDP_ENDLEVEL_PROBE_H
#define NET_UDP_ENDLEVEL_PROBE_H

/* Android introspection-only probe, included by each engine's UDP transport */
typedef struct net_udp_endlevel_probe {
	unsigned rejected, applied, current;
	int sending, countdown;
	fix64 next_send;
	int connected[MAX_PLAYERS], kills[MAX_PLAYERS], deaths[MAX_PLAYERS];
	short matrix[MAX_PLAYERS][MAX_PLAYERS];
} net_udp_endlevel_probe;

static inline void net_udp_endlevel_probe_arm(net_udp_endlevel_probe *probe)
{
	memset(probe, 0, sizeof(*probe));
	probe->countdown = Countdown_seconds_left;
	memcpy(probe->matrix, kill_matrix, sizeof(probe->matrix));
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		probe->connected[i] = Players[i].connected;
		probe->kills[i] = Players[i].net_kills_total;
		probe->deaths[i] = Players[i].net_killed_total;
	}
}

static inline int net_udp_endlevel_probe_preserved(const net_udp_endlevel_probe *probe)
{
	if (Countdown_seconds_left != probe->countdown ||
	    memcmp(probe->matrix, kill_matrix, sizeof(probe->matrix))) return 0;
	for (int i = 0; i < MAX_PLAYERS; ++i)
		if (probe->connected[i] != Players[i].connected ||
		    probe->kills[i] != Players[i].net_kills_total ||
		    probe->deaths[i] != Players[i].net_killed_total) return 0;
	return 1;
}

static inline int net_udp_endlevel_probe_ready(const net_udp_endlevel_probe *probe)
{
	return probe->rejected == 3 && !probe->applied && probe->current &&
	       net_udp_endlevel_probe_preserved(probe);
}

static inline void net_udp_endlevel_probe_receive(net_udp_endlevel_probe *probe,
                                                  const ubyte *data, int size, int allowed)
{
	if (!probe->sending) return;
	const int kills_offset = data[0] == UPID_ENDLEVEL_C ? 8 : 7 + Player_num * 5;
	if (GET_INTEL_SHORT(data + kills_offset) != (uint16_t) -30000) {
		if (allowed) ++probe->current;
		return;
	}
	if (allowed) {
		++probe->applied;
		return;
	}
	coop_gameplay_stamp sent, current = coop_gameplay_current_stamp();
	if (!coop_gameplay_stamp_read(&sent, data + size - COOP_GAMEPLAY_STAMP_BYTES,
	                              COOP_GAMEPLAY_STAMP_BYTES)) return;
	if (!current.frozen && sent.level == current.level) {
		if (!sent.frozen && sent.visit + 1 == current.visit) probe->rejected |= 1;
		if (sent.frozen && sent.visit == current.visit) probe->rejected |= 2;
	}
}

static inline int net_udp_endlevel_probe_clone(ubyte *copy, const ubyte *data, int size, int frozen)
{
	coop_gameplay_stamp stamp;
	if (size != (data[0] == UPID_ENDLEVEL_H ? UPID_ENDLEVEL_H_SIZE : UPID_ENDLEVEL_C_SIZE) ||
	    !coop_gameplay_stamp_read(&stamp, data + size - COOP_GAMEPLAY_STAMP_BYTES,
	                              COOP_GAMEPLAY_STAMP_BYTES) ||
	    !stamp.visit || stamp.frozen) return 0;
	memcpy(copy, data, size);
	if (data[0] == UPID_ENDLEVEL_H) {
		copy[5] = 0;
		for (int i = 0; i < MAX_PLAYERS; ++i) {
			copy[6 + i * 5] = CONNECT_DISCONNECTED;
			PUT_INTEL_SHORT(copy + 7 + i * 5, -30000);
			PUT_INTEL_SHORT(copy + 9 + i * 5, -30000);
		}
		memset(copy + 6 + MAX_PLAYERS * 5, 0x7f, MAX_PLAYERS * MAX_PLAYERS * 2);
	} else {
		copy[6] = CONNECT_DISCONNECTED;
		copy[7] = 0;
		PUT_INTEL_SHORT(copy + 8, -30000);
		PUT_INTEL_SHORT(copy + 10, -30000);
		memset(copy + 12, 0x7f, MAX_PLAYERS * 2);
	}
	if (frozen) stamp.frozen = 1;
	else --stamp.visit;
	return coop_gameplay_stamp_write(copy + size - COOP_GAMEPLAY_STAMP_BYTES,
	                                 COOP_GAMEPLAY_STAMP_BYTES, &stamp);
}

#endif
