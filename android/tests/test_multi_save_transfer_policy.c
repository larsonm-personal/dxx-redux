#include "multi_save_transfer_policy.h"

#include <stdio.h>

#define CHECK(condition)                                                     \
	do {                                                                     \
		if (!(condition)) {                                                  \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                             \
			return 1;                                                        \
		}                                                                    \
	} while (0)

int main(void)
{
	CHECK(multi_save_transfer_host_action_for_rewind(0) ==
	      MULTI_SAVE_TRANSFER_HOST_APPLY_NOW);
	CHECK(multi_save_transfer_host_action_for_rewind(1) ==
	      MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS);

	CHECK(multi_save_transfer_client_apply_action_for_context(
	          0, 1, 1, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 1, 963, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 0, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 1, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_APPLY);
	puts("multi save transfer policy tests passed");
	return 0;
}
