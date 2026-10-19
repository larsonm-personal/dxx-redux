#ifndef CLASSIC_DEMO_PLAYER_H
#define CLASSIC_DEMO_PLAYER_H

static inline int classic_demo_player_count_valid(int count, int max_players)
{
	return count > 0 && count <= max_players;
}

static inline int classic_demo_player_index_valid(int player_index, int max_players)
{
	return player_index >= 0 && player_index < max_players;
}

static inline int classic_demo_player_count_can_add(int count, int max_players)
{
	return classic_demo_player_count_valid(count, max_players) && count < max_players;
}

static inline int classic_demo_player_count_can_remove(int count, int max_players)
{
	return classic_demo_player_count_valid(count, max_players) && count > 1;
}

#endif
