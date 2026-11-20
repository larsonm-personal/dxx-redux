#include "net_udp_mission_filename.h"
#include <stdio.h>

#define REQUIRE(expr)                                         \
	do {                                                      \
		if (!(expr)) {                                        \
			fprintf(stderr, "Failed at line %d\n", __LINE__); \
			return 1;                                         \
		}                                                     \
	} while (0)

int main(void)
{
	enum { name_offset = 3,
		   fixed_size = 15 };
	unsigned char packet[fixed_size + NET_UDP_MISSION_NAME_CAPACITY + 1] = { 0 };
	unsigned char *extension = packet + fixed_size;
	char maximum[NET_UDP_MISSION_NAME_CAPACITY];
	const char *decoded = NULL;
	memcpy(packet + name_offset, "vignette", 9);
	REQUIRE(net_udp_write_mission_filename(extension, 0, "vignette") == 0);
	REQUIRE(net_udp_read_mission_filename(packet, fixed_size, fixed_size, name_offset, &decoded));
	REQUIRE(!strcmp(decoded, "vignette"));
	int n = net_udp_write_mission_filename(extension, sizeof(packet) - fixed_size, "vignette-xl");
	REQUIRE(n == 13);
	REQUIRE(net_udp_read_mission_filename(packet, fixed_size + n, fixed_size, name_offset, &decoded));
	REQUIRE(!strcmp(decoded, "vignette-xl"));
	REQUIRE(!net_udp_read_mission_filename(packet, fixed_size + n - 1, fixed_size, name_offset, &decoded));
	REQUIRE(!net_udp_read_mission_filename(packet, fixed_size + n + 1, fixed_size, name_offset, &decoded));
	extension[9] = '/';
	REQUIRE(!net_udp_read_mission_filename(packet, fixed_size + n, fixed_size, name_offset, &decoded));
	extension[9] = 0;
	REQUIRE(!net_udp_read_mission_filename(packet, fixed_size + n, fixed_size, name_offset, &decoded));
	memset(maximum, 'a', sizeof(maximum) - 1);
	maximum[sizeof(maximum) - 1] = 0;
	REQUIRE(net_udp_write_mission_filename(extension, sizeof(packet) - fixed_size - 1, maximum) < 0);
	n = net_udp_write_mission_filename(extension, sizeof(packet) - fixed_size, maximum);
	REQUIRE(n == sizeof(packet) - fixed_size);
	REQUIRE(!net_udp_read_mission_filename(packet, sizeof(packet), fixed_size, name_offset, &decoded));
	memcpy(packet + name_offset, maximum, 8);
	REQUIRE(net_udp_read_mission_filename(packet, sizeof(packet), fixed_size, name_offset, &decoded));
	REQUIRE(!strcmp(decoded, maximum));
	return 0;
}
