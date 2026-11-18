#ifndef XL_POWERUP_POLICY_H
#define XL_POWERUP_POLICY_H
#include "preview.h"

static inline int xl_debug_powerup_allowed(int id)
{
	// Native POW_MEGAWOW and pinned XL POW_MEGAWOW both use ID 27
	// XL initializes that pickup handler to NULL in every build configuration
	return id != 27 || !xl_level_extensions_active();
}
#endif
