#ifndef DXX_HEADLESS_DIAGNOSTICS_H
#define DXX_HEADLESS_DIAGNOSTICS_H

#include <stdio.h>
#include <string.h>
#include <set>
#include <string>

// Include before legacy engine headers that change structure packing
// Headless processes must never require acknowledgement through an OS dialog
static void headless_error(const char *message)
{
	fprintf(stderr, "%s\n", message);
	fflush(stderr);
}

static void headless_warning(char *message)
{
	static std::set<std::string> unknown_d1_textures;
	if (strstr(message, "can't convert unknown descent 1 texture #") &&
	    !unknown_d1_textures.insert(message).second)
		return;
	headless_error(message);
}

#endif
