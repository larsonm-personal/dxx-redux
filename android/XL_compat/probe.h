#ifndef DXX_XL_COMPAT_PROBE_H
#define DXX_XL_COMPAT_PROBE_H

#include "inventory.h"

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{

struct GameplayProbe {
	Bytes stock_level;
	Inventory source;
};

// Developer validation only, not mission admission
// Lighting remains in source and must be integrated before faithful play is claimed
GameplayProbe prepare_gameplay_probe(const Bytes &source, bool water_segments = false);
// Layout qualification only: runtime admission still rejects 17/36
// All semantic blockers (including no-damage and equipment generators) still apply
GameplayProbe prepare_serialization_probe_17_36(const Bytes &source);

} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
