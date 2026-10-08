#ifndef MULTI_GAMEPLAY_OPTIONS_H
#define MULTI_GAMEPLAY_OPTIONS_H

#include "pstypes.h"

void multi_do_reactor_pause(const ubyte *buf, int authenticated_sender);
void multi_do_matcen_mode(const ubyte *buf, int authenticated_sender);

#endif
