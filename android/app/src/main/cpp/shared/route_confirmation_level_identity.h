#pragma once

#include <string>
#include <vector>

#include <physfs.h>
#include "picosha2.h"

inline std::string readiness_level_source_sha256(const char *filename)
{
	if (!filename || !*filename) return {};
	PHYSFS_File *file = PHYSFS_openRead(filename);
	if (!file) return {};
	const PHYSFS_sint64 size = PHYSFS_fileLength(file);
	if (size < 0 || size > 64 * 1024 * 1024) {
		PHYSFS_close(file);
		return {};
	}
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	const bool complete = PHYSFS_readBytes(file, bytes.data(), size) == size;
	PHYSFS_close(file);
	return complete ? picosha2::hash256_hex_string(bytes) : std::string{};
}
