/* Optional launcher LAN admission, never a loaded-readiness assertion */
#ifndef DXX_NET_UDP_LAUNCH_ADMISSION_H
#define DXX_NET_UDP_LAUNCH_ADMISSION_H
#include <stddef.h>
#include <stdint.h>
enum { UPID_LAUNCH_ADMISSION = 36 };

static inline int android_net_udp_launch_admission_allowed(
    int launcher_lan, int auto_selecting, int cooperative, int starting,
    int observer, int requested_level, int selected_level)
{
	return launcher_lan && auto_selecting && cooperative && starting &&
	       !observer && requested_level > 0 && requested_level == selected_level;
}

/* Existing REQUEST transcripts remain unchanged. The extra signed purpose
 * prevents an admission datagram being retagged into a readiness REQUEST */
static inline size_t android_net_udp_launch_admission_bind_purpose(
    unsigned packet_type, uint8_t *payload, size_t size, size_t capacity)
{
	if (packet_type != UPID_LAUNCH_ADMISSION) return size;
	if (!payload || size >= capacity) return 0;
	payload[size] = UPID_LAUNCH_ADMISSION;
	return size + 1;
}
#endif
