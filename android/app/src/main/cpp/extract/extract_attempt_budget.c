#include "extract_attempt_budget.h"

#include <stddef.h>

void dxx_extract_attempt_budget_init_with_limits(dxx_extract_attempt_budget_t *budget,
                                                 uint64_t max_output_bytes,
                                                 uint64_t max_entries,
                                                 uint64_t max_memory_bytes,
                                                 dxx_extract_cancel_fn cancel,
                                                 void *cancel_user_data)
{
	if (!budget)
		return;
	budget->max_output_bytes = max_output_bytes;
	budget->max_entries = max_entries;
	budget->max_memory_bytes = max_memory_bytes;
	budget->output_bytes = 0;
	budget->entries = 0;
	budget->memory_bytes = 0;
	budget->cancelled = 0;
	budget->cancel = cancel;
	budget->cancel_user_data = cancel_user_data;
}

void dxx_extract_attempt_budget_init(dxx_extract_attempt_budget_t *budget,
                                     dxx_extract_cancel_fn cancel,
                                     void *cancel_user_data)
{
	dxx_extract_attempt_budget_init_with_limits(budget,
	                                            DXX_EXTRACT_MAX_TOTAL_BYTES,
	                                            DXX_EXTRACT_MAX_ENTRIES,
	                                            DXX_EXTRACT_MAX_MEMORY_BYTES,
	                                            cancel, cancel_user_data);
}

int dxx_extract_attempt_reserve_output(dxx_extract_attempt_budget_t *budget,
                                       const char *output_path,
                                       uint64_t bytes,
                                       uint64_t entries)
{
	uint64_t output_total;
	uint64_t entry_total;

	if (!budget || !output_path)
		return -1;
	if (dxx_extract_attempt_cancelled(budget))
		return DXX_EXTRACT_CANCELLED;
	output_total = budget->output_bytes;
	entry_total = budget->entries;
	if (dxx_extract_add_bytes(&output_total, bytes,
	                          budget->max_output_bytes) < 0 ||
	    dxx_extract_add_bytes(&entry_total, entries,
	                          budget->max_entries) < 0 ||
	    !dxx_extract_has_free_space(output_path, bytes))
		return -1;
	budget->output_bytes = output_total;
	budget->entries = entry_total;
	return 0;
}

int dxx_extract_attempt_reserve_memory(dxx_extract_attempt_budget_t *budget,
                                       uint64_t bytes)
{
	if (!budget ||
	    dxx_extract_add_bytes(&budget->memory_bytes, bytes,
	                          budget->max_memory_bytes) < 0)
		return -1;
	return 0;
}

void dxx_extract_attempt_release_memory(dxx_extract_attempt_budget_t *budget,
                                        uint64_t bytes)
{
	if (!budget)
		return;
	if (bytes > budget->memory_bytes)
		budget->memory_bytes = 0;
	else
		budget->memory_bytes -= bytes;
}

int dxx_extract_attempt_cancelled(dxx_extract_attempt_budget_t *budget)
{
	if (!budget)
		return 0;
	if (budget->cancelled)
		return 1;
	if (budget->cancel && budget->cancel(budget->cancel_user_data)) {
		budget->cancelled = 1;
		return 1;
	}
	return 0;
}

void dxx_extract_attempt_cancel(dxx_extract_attempt_budget_t *budget)
{
	if (budget)
		budget->cancelled = 1;
}
