#ifndef DXX_NET_UDP_MISSION_FILENAME_H
#define DXX_NET_UDP_MISSION_FILENAME_H

#include <string.h>

/* Full D2 game info and sync can append the catalog basename, including NUL.
 * The first byte gives its length excluding NUL. Discovery still uses 8.3 */
#define NET_UDP_MISSION_NAME_CAPACITY 256

static inline int net_udp_write_mission_filename(unsigned char *out, int capacity, const char *name)
{
	const size_t length = strlen(name);
	if (length <= 8) return 0;
	if (length >= NET_UDP_MISSION_NAME_CAPACITY || capacity < (int) length + 2) return -1;
	out[0] = (unsigned char) length;
	memcpy(out + 1, name, length + 1);
	return (int) length + 2;
}

static inline int net_udp_read_mission_filename(const unsigned char *packet, int size, int fixed_size,
                                                int name_offset, const char **name)
{
	if (!packet || size < fixed_size || name_offset < 0 || fixed_size - name_offset < 9) return 0;
	const char *legacy = (const char *) packet + name_offset;
	if (!memchr(legacy, 0, 9)) return 0;
	if (size == fixed_size) {
		*name = legacy;
		return 1;
	}
	const int length = packet[fixed_size];
	if (length <= 8 || size - fixed_size != length + 2) return 0;
	const char *full = (const char *) packet + fixed_size + 1;
	if (full[length] || memchr(full, 0, length) || strpbrk(full, "/\\:") ||
	    legacy[8] || memcmp(legacy, full, 8)) return 0;
	*name = full;
	return 1;
}
#endif
