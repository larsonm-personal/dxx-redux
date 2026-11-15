#ifndef DXX_XL_RETAINED_RESOURCE_H
#define DXX_XL_RETAINED_RESOURCE_H
#include <functional>
#include <vector>
#include "physfs.h"

// Retain one bounded source read, preserving the caller's file position
std::vector<unsigned char> xl_retain_resource(PHYSFS_file *file);
// Run existing native file codecs over the same retained bytes without publishing
// a resource basename. The private read-only mount exists only during the callback
void xl_read_retained_resource(const std::vector<unsigned char> &bytes, const std::function<void(PHYSFS_file *)> &reader);
#endif
