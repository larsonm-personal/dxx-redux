#ifndef DXX_XL_CLASSIC_SEGMENT_FUNCTION_H
#define DXX_XL_CLASSIC_SEGMENT_FUNCTION_H

static inline int xl_classic_native_segment_function(int level_version, int function, int multiplayer)
{
	/* Classic XL types 9 and 10 are team rooms and create no station */
	return level_version == 7 && !multiplayer && (function == 9 || function == 10) ? 0 : function;
}

#endif
