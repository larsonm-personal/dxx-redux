#ifndef MATCEN_MODE_H
#define MATCEN_MODE_H

#include <stdint.h>

#define MATCEN_MODE_MAX_CENTERS 20

enum {
	MATCEN_MODE_DEFAULT = 0,
	MATCEN_MODE_ONE_ROUND = 1,
	MATCEN_MODE_PAUSED = 2,
	MATCEN_MODE_COUNT = 3
};

void matcen_mode_reset_game(void);
void matcen_mode_reset_level(void);
int matcen_mode_get(void);
int matcen_mode_set(int mode);
int matcen_mode_can_activate(int matcen_num);
void matcen_mode_record_activation(int matcen_num);
void matcen_mode_get_activation_counts(uint8_t *counts, int count);
int matcen_mode_restore(int mode, const uint8_t *counts, int count);

#endif
