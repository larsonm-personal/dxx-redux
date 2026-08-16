#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "matcen_mode.h"

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	uint8_t saved_counts[MATCEN_MODE_MAX_CENTERS];
	int failures = 0;

	matcen_mode_reset_game();
	failures += expect(matcen_mode_get() == MATCEN_MODE_DEFAULT,
	                   "new game did not select default mode");
	matcen_mode_record_activation(2);
	failures += expect(matcen_mode_can_activate(2),
	                   "default mode changed activation limits");
	matcen_mode_set(MATCEN_MODE_ONE_ROUND);
	failures += expect(!matcen_mode_can_activate(2),
	                   "used matcen was not capped after enabling");
	failures += expect(matcen_mode_can_activate(3),
	                   "unused matcen was capped too early");
	matcen_mode_record_activation(3);
	failures += expect(!matcen_mode_can_activate(3),
	                   "one-round mode allowed a second activation");
	matcen_mode_set(MATCEN_MODE_PAUSED);
	failures += expect(!matcen_mode_can_activate(4),
	                   "paused mode allowed an unused matcen activation");
	matcen_mode_set(MATCEN_MODE_DEFAULT);
	failures += expect(matcen_mode_can_activate(2),
	                   "default mode did not restore ordinary limits");
	failures += expect(!matcen_mode_set(MATCEN_MODE_COUNT),
	                   "invalid mode was accepted");

	memset(saved_counts, 0, sizeof(saved_counts));
	matcen_mode_get_activation_counts(saved_counts, MATCEN_MODE_MAX_CENTERS);
	matcen_mode_reset_game();
	failures += expect(matcen_mode_restore(MATCEN_MODE_PAUSED, saved_counts,
	                                      MATCEN_MODE_MAX_CENTERS),
	                   "saved mode could not be restored");
	failures += expect(matcen_mode_get() == MATCEN_MODE_PAUSED,
	                   "saved paused mode was not restored");
	failures += expect(!matcen_mode_can_activate(2),
	                   "paused restore allowed a matcen activation");
	matcen_mode_set(MATCEN_MODE_ONE_ROUND);
	failures += expect(!matcen_mode_can_activate(2),
	                   "saved activation count was not restored");
	matcen_mode_reset_level();
	failures += expect(matcen_mode_get() == MATCEN_MODE_ONE_ROUND,
	                   "new level cleared the selected mode");
	failures += expect(matcen_mode_can_activate(2),
	                   "new level retained activation counts");

	if (failures)
		return 1;
	puts("PASS: matcen tri-state mode");
	return 0;
}
