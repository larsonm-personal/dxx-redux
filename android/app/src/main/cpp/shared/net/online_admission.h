#ifndef DXX_ONLINE_ADMISSION_H
#define DXX_ONLINE_ADMISSION_H
#include <string.h>

/* Local proxy envelope only, mirrored by OnlineAdmissionEnvelope.kt
 * Never serialized into the native network protocol or sent to another peer */
#define ONLINE_ADMISSION_HEADER     21
#define ONLINE_ADMISSION_PROXY_PORT 42430
static inline int online_admission_present(const unsigned char id[16])
{
	unsigned any = 0;
	for (int i = 0; i < 16; ++i) any |= id[i];
	return any != 0;
}
static inline int online_admission_unwrap(const unsigned char *data, int size,
                                          int loopback, unsigned port,
                                          unsigned char id[16], unsigned char *slot)
{
	if (size < 4 || memcmp(data, "DXOA", 4)) return 0;
	if (size <= ONLINE_ADMISSION_HEADER || !loopback || data[4] < 1 || data[4] > 7 ||
	    port != ONLINE_ADMISSION_PROXY_PORT + data[4] ||
	    data[ONLINE_ADMISSION_HEADER] != 8 || !online_admission_present(data + 5)) return -1;
	memcpy(id, data + 5, 16);
	*slot = data[4];
	return ONLINE_ADMISSION_HEADER;
}
#endif
