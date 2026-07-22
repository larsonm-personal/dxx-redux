#include <stdint.h>
#include <stdio.h>

#include "extract_limits.h"

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

int main(void)
{
	uint64_t total;
	int failures = 0;

	total = DXX_EXTRACT_MAX_TOTAL_BYTES - 1u;
	failures += expect(dxx_extract_add_bytes(&total, 1u,
	                                         DXX_EXTRACT_MAX_TOTAL_BYTES) == 0,
	                   "aggregate exact limit");
	failures += expect(total == DXX_EXTRACT_MAX_TOTAL_BYTES,
	                   "aggregate exact limit value");
	failures += expect(dxx_extract_add_bytes(&total, 1u,
	                                         DXX_EXTRACT_MAX_TOTAL_BYTES) < 0,
	                   "aggregate one over limit");
	failures += expect(dxx_extract_entry_allowed(DXX_EXTRACT_MAX_ENTRY_BYTES,
	                                             DXX_EXTRACT_MAX_ENTRY_BYTES) != 0,
	                   "entry exact byte limit");
	failures += expect(dxx_extract_entry_allowed(DXX_EXTRACT_MAX_ENTRY_BYTES + 1u,
	                                             DXX_EXTRACT_MAX_ENTRY_BYTES + 1u) == 0,
	                   "entry one over byte limit");
	failures += expect(dxx_extract_entry_allowed(DXX_EXTRACT_MAX_ENTRY_BYTES, 1u) == 0,
	                   "entry ratio over limit");
	failures += expect(dxx_extract_entry_allowed(DXX_EXTRACT_MAX_RATIO, 1u) != 0,
	                   "entry ratio exact limit");
	failures += expect(dxx_extract_entry_allowed(DXX_EXTRACT_MAX_RATIO + 1u, 1u) == 0,
	                   "entry ratio one over limit");
	failures += expect(dxx_extract_entry_allowed(1u, 0u) == 0,
	                   "nonempty entry without compressed size");
	failures += expect(dxx_extract_memory_allowed(DXX_EXTRACT_MAX_MEMORY_BYTES, 0u) != 0,
	                   "memory exact limit");
	failures += expect(dxx_extract_memory_allowed(DXX_EXTRACT_MAX_MEMORY_BYTES, 1u) == 0,
	                   "memory one over limit");

	if (failures != 0)
		return 1;
	printf("Extraction limit tests passed\n");
	return 0;
}
