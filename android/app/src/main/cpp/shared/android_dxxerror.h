#ifndef ANDROID_DXXERROR_H
#define ANDROID_DXXERROR_H

#include "android_crash_handler.h"
#include "android_log.h"

#undef Assert
#ifdef NDEBUG
/* Non-debug builds log failed assertions to launcher-exportable Game Logs and
 * continue execution. */
#define Assert(expr)                                                          \
	(!(expr) ? (debug_log(DLOG_GAME,                                          \
	                      "ASSERT FAIL, logging and continuing: %s at %s:%d", \
	                      #expr, __FILE__, __LINE__),                         \
	            (void) 0)                                                     \
	         : (void) 0)
#else
#define Assert(expr)                                           \
	(!(expr) ? (crash_breadcrumb_v("ASSERT FAIL: %s at %s:%d", \
	                               #expr, __FILE__, __LINE__), \
	            assert(expr))                                  \
	         : (void) 0)
#endif
#define Int3() crash_breadcrumb_v("Int3 at %s:%d", __FILE__, __LINE__)

#endif
