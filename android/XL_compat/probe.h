#ifndef DXX_XL_COMPAT_PROBE_H
#define DXX_XL_COMPAT_PROBE_H

#include "inventory.h"

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{

struct GameplayProbe {
	// Native adapter bytes; wide walls use the owned private mine encoding
	// These bytes alone are not a standalone stock-compatible level
	Bytes stock_level;
	Inventory source;
};

// Translate only supported layouts and decoded feature requirements
// Lighting remains in source and must be integrated before faithful play is claimed
GameplayProbe prepare_gameplay_probe(const Bytes &source, bool water_segments = false);
// Explicit 17/36 entry point for layout qualification fixtures
// All unsupported semantic requirements, including equipment generators, still apply
GameplayProbe prepare_serialization_probe_17_36(const Bytes &source);
// Explicit 23/40 entry point for layout qualification fixtures
GameplayProbe prepare_serialization_probe_23_40(const Bytes &source);

} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
