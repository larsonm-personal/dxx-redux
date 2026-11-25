#ifndef DXX_XL_ADMISSION_H
#define DXX_XL_ADMISSION_H
#include "inventory.h"
namespace xl_compat
{
bool classic_effect_layout(const Inventory &level);
bool default_legacy_water_profile(const Inventory &level);
bool legacy_water_has_actors(const Inventory &level);
// Format/feature eligibility only; the engine must also validate mounted assets
std::vector<Issue> runtime_blockers(const Inventory &level, bool experimental_water = false, bool explicit_geometry = true);
bool has_explicit_geometry(const Inventory &level);
std::string blocker_message(const std::vector<Issue> &blockers);
std::vector<int> object_chain_targets(const Inventory &level, const Trigger &master);
} // namespace xl_compat
#endif
