#ifndef DXX_ANDROID_EXTRACT_LIMITS_H
#define DXX_ANDROID_EXTRACT_LIMITS_H

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/statvfs.h>
#endif

#define DXX_EXTRACT_MAX_ENTRY_BYTES    (512ULL * 1024ULL * 1024ULL)
#define DXX_EXTRACT_MAX_TOTAL_BYTES    (2ULL * 1024ULL * 1024ULL * 1024ULL)
#define DXX_EXTRACT_MAX_MEMORY_BYTES   (128ULL * 1024ULL * 1024ULL)
#define DXX_EXTRACT_MAX_METADATA_BYTES (64ULL * 1024ULL * 1024ULL)
#define DXX_EXTRACT_MAX_ENTRIES        4096U
#define DXX_EXTRACT_MAX_RATIO          1000ULL
#define DXX_EXTRACT_FREE_HEADROOM      (50ULL * 1024ULL * 1024ULL)

static inline int dxx_extract_add_bytes(uint64_t *total, uint64_t value, uint64_t limit)
{
	if (!total || value > limit || *total > limit - value)
		return -1;
	*total += value;
	return 0;
}

static inline int dxx_extract_ratio_allowed(uint64_t expanded, uint64_t compressed)
{
	uint64_t quotient;

	if (expanded == 0)
		return 1;
	if (compressed == 0)
		return 0;
	quotient = expanded / compressed;
	return quotient < DXX_EXTRACT_MAX_RATIO ||
	       (quotient == DXX_EXTRACT_MAX_RATIO && expanded % compressed == 0);
}

static inline int dxx_extract_entry_allowed(uint64_t expanded, uint64_t compressed)
{
	return expanded <= DXX_EXTRACT_MAX_ENTRY_BYTES &&
	       dxx_extract_ratio_allowed(expanded, compressed);
}

static inline int dxx_extract_memory_allowed(uint64_t first, uint64_t second)
{
	return first <= DXX_EXTRACT_MAX_MEMORY_BYTES &&
	       second <= DXX_EXTRACT_MAX_MEMORY_BYTES - first;
}

static inline int dxx_extract_has_free_space(const char *path, uint64_t bytes)
{
	char existing[1024];
	char *slash;
	uint64_t required;

	if (!path || dxx_extract_add_bytes(&bytes, DXX_EXTRACT_FREE_HEADROOM,
	                                   UINT64_MAX) < 0)
		return 0;
	if (strlen(path) >= sizeof(existing))
		return 0;
	memcpy(existing, path, strlen(path) + 1u);
	for (;;) {
#ifdef _WIN32
		if (GetFileAttributesA(existing) != INVALID_FILE_ATTRIBUTES)
			break;
#else
		struct stat path_stats;
		if (stat(existing, &path_stats) == 0)
			break;
#endif
		slash = strrchr(existing, '/');
#ifdef _WIN32
		{
			char *backslash = strrchr(existing, '\\');
			if (!slash || (backslash && backslash > slash))
				slash = backslash;
		}
#endif
		if (!slash) {
			if (strcmp(existing, ".") == 0)
				return 0;
			existing[0] = '.';
			existing[1] = '\0';
			continue;
		}
		if (slash == existing ||
		    (slash == existing + 2 && existing[1] == ':'))
			slash[1] = '\0';
		else
			*slash = '\0';
		if (!existing[0])
			return 0;
	}
	required = bytes;
#ifdef _WIN32
	{
		ULARGE_INTEGER available;

		return GetDiskFreeSpaceExA(existing, &available, NULL, NULL) != 0 &&
		       available.QuadPart >= required;
	}
#else
	{
		struct statvfs stats;
		uint64_t available;

		if (statvfs(existing, &stats) != 0)
			return 0;
		if (stats.f_bavail != 0 && stats.f_frsize > UINT64_MAX / stats.f_bavail)
			available = UINT64_MAX;
		else
			available = (uint64_t) stats.f_bavail * (uint64_t) stats.f_frsize;
		return available >= required;
	}
#endif
}

#endif /* DXX_ANDROID_EXTRACT_LIMITS_H */
