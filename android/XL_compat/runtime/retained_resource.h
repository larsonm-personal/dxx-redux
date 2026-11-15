#ifndef DXX_XL_RETAINED_RESOURCE_H
#define DXX_XL_RETAINED_RESOURCE_H
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include "physfs.h"

// Retain one bounded source read, preserving the caller's file position
std::vector<unsigned char> xl_retain_resource(PHYSFS_file *file);
// Run existing native file codecs over the same retained bytes without publishing
// a resource basename. The private read-only mount exists only during the callback
void xl_read_retained_resource(const std::vector<unsigned char> &bytes, const std::function<void(PHYSFS_file *)> &reader);
// Read a captured flat member from its exact physical directory/archive origin,
// including when a different installed mission shadows the logical basename
std::vector<unsigned char> xl_retain_origin_resource(const std::string &origin, const std::string &member);
// Decode archive membership with PhysicsFS under a temporary private prefix
std::optional<std::vector<unsigned char>> xl_find_archive_resource(const std::vector<unsigned char> &archive, const std::string &extension, const std::string &member, std::size_t limit);
#endif
