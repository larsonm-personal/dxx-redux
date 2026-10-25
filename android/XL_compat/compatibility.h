#ifndef DXX_XL_COMPAT_COMPATIBILITY_H
#define DXX_XL_COMPAT_COMPATIBILITY_H
#include "inventory.h"
namespace xl_compat
{
// Metadata evidence only; never enables a level or changes engine admission
std::string compatibility_json(const std::string &name, const Bytes &bytes);
struct PackageResource {
	std::string path, sha256;
};
// A caller-supplied owned catalog, not a directory scan or runtime selection
std::string package_compatibility_json(const std::string &owner, const std::string &descriptor,
                                       const std::vector<PackageResource> &resources);
} // namespace xl_compat
#endif
