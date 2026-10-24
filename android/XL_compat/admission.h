#ifndef DXX_XL_ADMISSION_H
#define DXX_XL_ADMISSION_H
#include "inventory.h"
namespace xl_compat
{
// Format/feature eligibility only; the engine must also validate mounted assets
std::vector<Issue> runtime_blockers(const Inventory &level);
std::string blocker_message(const std::vector<Issue> &blockers);
} // namespace xl_compat
#endif
