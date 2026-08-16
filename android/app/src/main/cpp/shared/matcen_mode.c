#include "matcen_mode.h"

#include <string.h>

static uint8_t Matcen_mode;
static uint8_t Matcen_activation_counts[MATCEN_MODE_MAX_CENTERS];

void matcen_mode_reset_game(void)
{
	Matcen_mode = MATCEN_MODE_DEFAULT;
	matcen_mode_reset_level();
}

void matcen_mode_reset_level(void)
{
	memset(Matcen_activation_counts, 0, sizeof(Matcen_activation_counts));
}

int matcen_mode_get(void)
{
	return Matcen_mode;
}

int matcen_mode_set(int mode)
{
	if (mode < MATCEN_MODE_DEFAULT || mode >= MATCEN_MODE_COUNT)
		return 0;
	Matcen_mode = (uint8_t) mode;
	return 1;
}

int matcen_mode_can_activate(int matcen_num)
{
	if (matcen_num < 0 || matcen_num >= MATCEN_MODE_MAX_CENTERS)
		return 0;
	if (Matcen_mode == MATCEN_MODE_PAUSED)
		return 0;
	return Matcen_mode != MATCEN_MODE_ONE_ROUND ||
	       !Matcen_activation_counts[matcen_num];
}

void matcen_mode_record_activation(int matcen_num)
{
	if (matcen_num < 0 || matcen_num >= MATCEN_MODE_MAX_CENTERS)
		return;
	if (Matcen_activation_counts[matcen_num] != UINT8_MAX)
		Matcen_activation_counts[matcen_num]++;
}

void matcen_mode_get_activation_counts(uint8_t *counts, int count)
{
	if (!counts || count <= 0)
		return;
	if (count > MATCEN_MODE_MAX_CENTERS)
		count = MATCEN_MODE_MAX_CENTERS;
	memcpy(counts, Matcen_activation_counts, (size_t) count);
}

int matcen_mode_restore(int mode, const uint8_t *counts, int count)
{
	if (!matcen_mode_set(mode))
		return 0;
	matcen_mode_reset_level();
	if (!counts || count <= 0)
		return 1;
	if (count > MATCEN_MODE_MAX_CENTERS)
		count = MATCEN_MODE_MAX_CENTERS;
	memcpy(Matcen_activation_counts, counts, (size_t) count);
	return 1;
}
