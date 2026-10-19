#ifndef CLASSIC_DEMO_LASER_H
#define CLASSIC_DEMO_LASER_H

static inline int classic_demo_laser_level_valid(int laser_level, int maximum_laser_level)
{
	return laser_level >= 0 && laser_level <= maximum_laser_level;
}

static inline int classic_demo_laser_event_valid(int old_laser_level, int new_laser_level,
                                                 int maximum_laser_level)
{
	return classic_demo_laser_level_valid(old_laser_level, maximum_laser_level) &&
	       classic_demo_laser_level_valid(new_laser_level, maximum_laser_level);
}

#endif
