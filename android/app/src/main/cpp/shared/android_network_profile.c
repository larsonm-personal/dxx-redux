#include "android_network_profile.h"

#include <stdio.h>
#include <string.h>

void android_network_profile_record(struct android_network_frame *frame, int stage,
                                    const struct android_network_sample *sample)
{
	struct android_network_metric *metric;
	int i;
	if (stage < 0 || stage >= ANDROID_NETWORK_STAGE_COUNT || sample->wall_us < 0)
		return;
	metric = &frame->stage[stage];
	metric->wall_us += sample->wall_us;
	if (sample->cpu_us < 0 || metric->cpu_us < 0)
		metric->cpu_us = -1;
	else
		metric->cpu_us += sample->cpu_us;
	metric->calls++;
	if (sample->result < 0)
		metric->errors++;
	if (metric->calls == 1 || sample->wall_us > metric->worst.wall_us)
		metric->worst = *sample;
	if (stage != ANDROID_NETWORK_DISPATCH)
		return;
	for (i = 0; i < ANDROID_NETWORK_SLOW_PACKETS; i++) {
		if (i < frame->packet_count && sample->wall_us <= frame->packets[i].wall_us)
			continue;
		if (i + 1 < ANDROID_NETWORK_SLOW_PACKETS)
			memmove(&frame->packets[i + 1], &frame->packets[i],
			        (ANDROID_NETWORK_SLOW_PACKETS - i - 1) * sizeof(frame->packets[0]));
		frame->packets[i] = *sample;
		if (frame->packet_count < ANDROID_NETWORK_SLOW_PACKETS)
			frame->packet_count++;
		break;
	}
}

void android_network_profile_format(char *line, size_t capacity, const char *role,
                                    uint32_t frame_id, const struct android_network_frame *frame)
{
	static const char *const names[ANDROID_NETWORK_STAGE_COUNT] = {
		"poll", "receive", "dispatch", "send", "traffic_log", "listen"
	};
	int i;
	if (!capacity)
		return;
	for (i = 0; i < ANDROID_NETWORK_STAGE_COUNT; i++) {
		const struct android_network_metric *metric = &frame->stage[i];
		const struct android_network_sample *worst = &metric->worst;
		const size_t used = strlen(line);
		if (!metric->calls)
			continue;
		snprintf(line + used, capacity - used,
		         "stutter_v=1 type=net_stage role=%s frame=%u stage=%s calls=%u errors=%u wall_us=%lld cpu_us=%lld max_wall_us=%lld max_cpu_us=%lld max_socket=%d max_packet=%d max_name=%s max_bytes=%d max_result=%d max_errno=%d\n",
		         role, frame_id, names[i], metric->calls, metric->errors,
		         (long long) metric->wall_us, (long long) metric->cpu_us,
		         (long long) worst->wall_us, (long long) worst->cpu_us,
		         worst->socket_id, worst->packet_type, worst->packet_name,
		         worst->bytes, worst->result, worst->error);
	}
	for (i = 0; i < frame->packet_count; i++) {
		const struct android_network_sample *packet = &frame->packets[i];
		const size_t used = strlen(line);
		snprintf(line + used, capacity - used,
		         "stutter_v=1 type=net_packet role=%s frame=%u rank=%d packet=%d name=%s socket=%d bytes=%d wall_us=%lld cpu_us=%lld\n",
		         role, frame_id, i + 1, packet->packet_type,
		         packet->packet_name, packet->socket_id, packet->bytes,
		         (long long) packet->wall_us, (long long) packet->cpu_us);
	}
}
