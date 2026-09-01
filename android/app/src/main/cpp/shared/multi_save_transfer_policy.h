#ifndef MULTI_SAVE_TRANSFER_POLICY_H
#define MULTI_SAVE_TRANSFER_POLICY_H

typedef enum multi_save_transfer_host_action {
	MULTI_SAVE_TRANSFER_HOST_APPLY_NOW = 0,
	MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS = 1
} multi_save_transfer_host_action;

typedef enum multi_save_transfer_client_apply_action {
	MULTI_SAVE_TRANSFER_CLIENT_WAIT = 0,
	MULTI_SAVE_TRANSFER_CLIENT_APPLY = 1
} multi_save_transfer_client_apply_action;

static inline multi_save_transfer_host_action
multi_save_transfer_host_action_for_rewind(int has_connected_clients)
{
	return has_connected_clients
	           ? MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS
	           : MULTI_SAVE_TRANSFER_HOST_APPLY_NOW;
}

static inline multi_save_transfer_client_apply_action
multi_save_transfer_client_apply_action_for_context(int at_frame_boundary,
                                                    int active,
                                                    int apply_pending,
                                                    int chunks_received,
                                                    int total_chunks)
{
	return at_frame_boundary && active && apply_pending && total_chunks > 0 &&
	               chunks_received == total_chunks
	           ? MULTI_SAVE_TRANSFER_CLIENT_APPLY
	           : MULTI_SAVE_TRANSFER_CLIENT_WAIT;
}

#endif
