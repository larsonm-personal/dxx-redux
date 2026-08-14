#include <stdint.h>
#include <stdio.h>

#include "extract_limits.h"
#include "extract_attempt_budget.h"

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

int main(void)
{
	dxx_extract_attempt_budget_t budget;
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

	dxx_extract_attempt_budget_init_with_limits(&budget, 5, 2, 7, NULL, NULL);
	failures += expect(dxx_extract_attempt_reserve_output(&budget, ".", 2, 1) == 0,
	                   "attempt first output reservation");
	failures += expect(dxx_extract_attempt_reserve_output(&budget, ".", 3, 1) == 0,
	                   "attempt exact aggregate output and entry limits");
	failures += expect(budget.output_bytes == 5 && budget.entries == 2,
	                   "attempt exact aggregate values");
	failures += expect(dxx_extract_attempt_reserve_output(&budget, ".", 1, 0) < 0,
	                   "attempt output one over limit");
	failures += expect(dxx_extract_attempt_reserve_output(&budget, ".", 0, 1) < 0,
	                   "attempt entry one over limit");
	failures += expect(budget.output_bytes == 5 && budget.entries == 2,
	                   "rejected output reservation leaves state unchanged");
	failures += expect(dxx_extract_attempt_reserve_memory(&budget, 7) == 0,
	                   "attempt memory exact limit");
	failures += expect(dxx_extract_attempt_reserve_memory(&budget, 1) < 0,
	                   "attempt memory one over limit");
	dxx_extract_attempt_release_memory(&budget, 7);
	failures += expect(budget.memory_bytes == 0,
	                   "attempt memory releases after operation");
	dxx_extract_attempt_cancel(&budget);
	failures += expect(dxx_extract_attempt_cancelled(&budget) != 0,
	                   "attempt cancellation remains set");
	failures += expect(dxx_extract_attempt_reserve_output(&budget, ".", 0, 0) ==
	                       DXX_EXTRACT_CANCELLED,
	                   "attempt cancellation stops nested operation");

	if (failures != 0)
		return 1;
	printf("Extraction limit tests passed\n");
	return 0;
}
