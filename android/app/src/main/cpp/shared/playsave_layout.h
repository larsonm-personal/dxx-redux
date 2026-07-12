#ifndef PLAYSAVE_LAYOUT_H
#define PLAYSAVE_LAYOUT_H

#include <stddef.h>
#include <stdio.h>

struct playsave_binary_layout {
	long keysettings;
	long mouse;
	long control_dos;
	long control_win;
	long sensitivity;
	long weapon_order;
	int version;
	int byte_swapped;
};

int playsave_d1_get_layout(FILE *f, unsigned int save_file_id,
                           int compatible_saved_version, int compatible_player_version,
                           int max_missions, size_t hli_size, size_t saved_games_size,
                           size_t max_controls, struct playsave_binary_layout *layout);
int playsave_d2_get_layout(FILE *f, unsigned int save_file_id,
                           int compatible_version, int max_missions, size_t hli_size,
                           size_t max_controls, size_t max_message_len,
                           struct playsave_binary_layout *layout);

#endif
