#ifndef DXX_ANDROID_EXTRACT_ATTEMPT_BUDGET_H
#define DXX_ANDROID_EXTRACT_ATTEMPT_BUDGET_H

#include <stdint.h>

#include "extract_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*dxx_extract_cancel_fn)(void *user_data);

typedef struct dxx_extract_attempt_budget_t {
	uint64_t max_output_bytes;
	uint64_t max_entries;
	uint64_t max_memory_bytes;
	uint64_t output_bytes;
	uint64_t entries;
	uint64_t memory_bytes;
	int cancelled;
	dxx_extract_cancel_fn cancel;
	void *cancel_user_data;
} dxx_extract_attempt_budget_t;

void dxx_extract_attempt_budget_init(dxx_extract_attempt_budget_t *budget,
                                     dxx_extract_cancel_fn cancel,
                                     void *cancel_user_data);

void dxx_extract_attempt_budget_init_with_limits(dxx_extract_attempt_budget_t *budget,
                                                 uint64_t max_output_bytes,
                                                 uint64_t max_entries,
                                                 uint64_t max_memory_bytes,
                                                 dxx_extract_cancel_fn cancel,
                                                 void *cancel_user_data);

int dxx_extract_attempt_reserve_output(dxx_extract_attempt_budget_t *budget,
                                       const char *output_path,
                                       uint64_t bytes,
                                       uint64_t entries);

int dxx_extract_attempt_reserve_memory(dxx_extract_attempt_budget_t *budget,
                                       uint64_t bytes);

void dxx_extract_attempt_release_memory(dxx_extract_attempt_budget_t *budget,
                                        uint64_t bytes);

int dxx_extract_attempt_cancelled(dxx_extract_attempt_budget_t *budget);

void dxx_extract_attempt_cancel(dxx_extract_attempt_budget_t *budget);

#ifdef __cplusplus
}
#endif

#endif
