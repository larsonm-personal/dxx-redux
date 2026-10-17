#ifndef DXX_MATCEN_REGRESSION_CAPTURE_H
#define DXX_MATCEN_REGRESSION_CAPTURE_H

/* Observational load-time copies only. Never used to choose gameplay bindings.
 * Keep raw values before legacy loading overwrites redundant indices. */
#include <string.h>
#include "fuelcen.h"
#include "switch.h"

typedef struct matcen_authored_segment {
	int segment, ordinal, raw_matcen, raw_station;
} matcen_authored_segment;
typedef struct matcen_regression_capture {
	int generated_count, disk_count, declared_disk_count, trigger_count, complete, overflow;
	matcen_authored_segment segments[MAX_ROBOT_CENTERS];
	matcen_info records[MAX_ROBOT_CENTERS];
	trigger triggers[MAX_TRIGGERS];
} matcen_regression_capture;
extern matcen_regression_capture Matcen_regression_capture;

static inline void matcen_capture_reset(void)
{
	memset(&Matcen_regression_capture, 0, sizeof(Matcen_regression_capture));
}
static inline void matcen_capture_segment(int segment, int ordinal, int raw_matcen, int raw_station)
{
	matcen_regression_capture *c = &Matcen_regression_capture;
	if (c->generated_count >= MAX_ROBOT_CENTERS) {
		c->overflow = 1;
		return;
	}
	matcen_authored_segment *row = &c->segments[c->generated_count++];
	row->segment = segment;
	row->ordinal = ordinal;
	row->raw_matcen = raw_matcen;
	row->raw_station = raw_station;
}
static inline void matcen_capture_triggers(void)
{
	matcen_regression_capture *c = &Matcen_regression_capture;
	if (Num_triggers < 0 || Num_triggers > MAX_TRIGGERS) {
		c->overflow = 1;
		return;
	}
	c->trigger_count = Num_triggers;
	memcpy(c->triggers, Triggers, sizeof(trigger) * Num_triggers);
}
static inline void matcen_capture_record(int index)
{
	matcen_regression_capture *c = &Matcen_regression_capture;
	if (index < 0 || index >= MAX_ROBOT_CENTERS) {
		c->overflow = 1;
		return;
	}
	c->records[index] = RobotCenters[index];
	c->disk_count = index + 1;
}
#endif
