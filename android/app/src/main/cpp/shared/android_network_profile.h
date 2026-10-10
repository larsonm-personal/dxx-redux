#ifndef ANDROID_NETWORK_PROFILE_H
#define ANDROID_NETWORK_PROFILE_H

#include <stdint.h>
#include <stddef.h>

enum android_network_stage {
	ANDROID_NETWORK_POLL,
	ANDROID_NETWORK_RECEIVE,
	ANDROID_NETWORK_DISPATCH,
	ANDROID_NETWORK_SEND,
	ANDROID_NETWORK_TRAFFIC_LOG,
	ANDROID_NETWORK_LISTEN,
	ANDROID_NETWORK_STAGE_COUNT
};

#define ANDROID_NETWORK_SLOW_PACKETS 3

struct android_network_stamp {
	int64_t wall_us;
	int64_t cpu_us;
};

struct android_network_sample {
	int64_t wall_us;
	int64_t cpu_us; /* -1 means the thread CPU clock was unavailable */
	int socket_id;
	int packet_type;
	int bytes;
	int result;
	int error;
	char packet_name[32];
};

struct android_network_metric {
	int64_t wall_us;
	int64_t cpu_us;
	uint32_t calls;
	uint32_t errors;
	struct android_network_sample worst;
};

struct android_network_frame {
	struct android_network_metric stage[ANDROID_NETWORK_STAGE_COUNT];
	struct android_network_sample packets[ANDROID_NETWORK_SLOW_PACKETS];
	int packet_count;
};

void android_network_profile_record(struct android_network_frame *frame, int stage,
                                    const struct android_network_sample *sample);
/* Append bounded diagnostic lines to an already NUL-terminated report */
void android_network_profile_format(char *line, size_t capacity, const char *role,
                                    uint32_t frame_id, const struct android_network_frame *frame);

#endif
