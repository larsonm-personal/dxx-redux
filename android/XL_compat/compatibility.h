#ifndef DXX_XL_COMPAT_COMPATIBILITY_H
#define DXX_XL_COMPAT_COMPATIBILITY_H
#include "inventory.h"
namespace xl_compat
{
// Metadata evidence only; never enables a level or changes engine admission
std::string compatibility_json(const std::string &name, const Bytes &bytes);
} // namespace xl_compat
#endif
