#ifndef ANDROID_DXXERROR_H
#define ANDROID_DXXERROR_H

#include "android_crash_handler.h"

#undef Assert
#define Assert(expr) \
	(!(expr) ? (crash_breadcrumb_v("ASSERT FAIL: %s at %s:%d", \
		#expr, __FILE__, __LINE__), assert(expr)) : (void)0)
#define Int3() crash_breadcrumb_v("Int3 at %s:%d", __FILE__, __LINE__)

#endif
