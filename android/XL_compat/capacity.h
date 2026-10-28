#ifndef DXX_XL_CAPACITY_H
#define DXX_XL_CAPACITY_H

#define DXX_CLASSIC_MAX_OBJECTS   1000
#define DXX_XL_MAX_OBJECTS        32768
#define DXX_CLASSIC_MAX_WALLS     255
#define DXX_XL_MAX_WALLS          2047
#define DXX_XL_MAX_CLOAKING_WALLS 100
#define DXX_CLASSIC_MAX_TRIGGERS  100
#define DXX_XL_MAX_TRIGGERS       254
/* Private translated mine with 16-bit wall references, never a public dialect */
#define DXX_XL_WIDE_WALL_MINE 0x58

/* Source runtime headroom depends on the authored table, not MAX_OBJECTS_D2X */
static inline unsigned long long xl_object_capacity_for_count(unsigned long long count)
{
	return count < 1000 ? count + 1000 : count * 2;
}

#endif
