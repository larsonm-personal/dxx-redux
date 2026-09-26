/* Asset admission for D2 network sessions, including native D1 gameplay */
#ifndef D1_IN_D2_NET_H
#define D1_IN_D2_NET_H

#include "pstypes.h"

/* Length byte plus base/custom and optional source digests; zero means D2 */
#define D1_IN_D2_NET_ASSET_SIZE 65
void d1_in_d2_write_network_identity(ubyte record[D1_IN_D2_NET_ASSET_SIZE]);
const char *d1_in_d2_check_network_identity(const ubyte *record, int size);

#endif
