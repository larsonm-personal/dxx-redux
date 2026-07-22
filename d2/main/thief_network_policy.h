#ifndef _THIEF_NETWORK_POLICY_H
#define _THIEF_NETWORK_POLICY_H

#define THIEF_NETWORK_NO_MODE 255
#define THIEF_STATE_CONTACT 1

static inline int thief_network_mode_is_valid(int mode, int attack_mode,
	int retreat_mode, int wait_mode)
{
	return mode == attack_mode || mode == retreat_mode || mode == wait_mode;
}

static inline int thief_network_should_prepare_path(int contact,
	int remote_owner, int player_num)
{
	return contact && remote_owner == player_num;
}

#endif
