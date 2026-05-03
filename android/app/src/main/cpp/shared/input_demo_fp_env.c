#include "input_demo_fp_env.h"

#include <stdio.h>

#if defined(_WIN32) && defined(_MSC_VER)
#include <float.h>
#include <xmmintrin.h>
#else
#include <fenv.h>
#endif

static int write_error(char *error, size_t error_size, const char *message)
{
	if (error && error_size > 0) {
		snprintf(error, error_size, "%s", message);
	}
	return 0;
}

int input_demo_configure_startup_fp_environment(char *error, size_t error_size)
{
#if defined(_WIN32) && defined(_MSC_VER)
	unsigned int control_word = 0;

#if defined(_M_IX86)
	if (_controlfp_s(&control_word, _PC_53, _MCW_PC) != 0)
		return write_error(error, error_size, "Failed to set floating point precision");
#endif
	if (_controlfp_s(&control_word, _RC_NEAR, _MCW_RC) != 0)
		return write_error(error, error_size, "Failed to set floating point rounding mode");
	if (_controlfp_s(&control_word, 0, 0) != 0)
		return write_error(error, error_size, "Failed to read floating point control word");
	if ((control_word & _MCW_RC) != _RC_NEAR)
		return write_error(error, error_size, "Floating point rounding mode is not round-to-nearest");
#if defined(_M_IX86)
	if ((control_word & _MCW_PC) != _PC_53)
		return write_error(error, error_size, "Floating point precision is not 53-bit");
#endif
#elif defined(FE_TONEAREST)
	if (fesetround(FE_TONEAREST) != 0)
		return write_error(error, error_size, "Failed to set floating point rounding mode");
	if (fegetround() != FE_TONEAREST)
		return write_error(error, error_size, "Floating point rounding mode is not round-to-nearest");
#else
	(void) error;
	(void) error_size;
#endif

	return 1;
}

void input_demo_restore_replay_fp_environment(void)
{
#if defined(_WIN32) && defined(_MSC_VER)
	unsigned int control_word = 0;

#if defined(_M_IX86)
	_controlfp_s(&control_word, _PC_53, _MCW_PC);
#endif
	_controlfp_s(&control_word, _RC_NEAR, _MCW_RC);
	/* 0x1f80 is the SSE MXCSR reset/default value:
	 * - all FP exceptions masked
	 * - round-to-nearest-even
	 * - flush-to-zero and denormals-are-zero disabled
	 * We restore it on replay frames so deterministic host replays cannot drift if
	 * external code changed MXCSR before entering the game loop.
	 * Android builds do not use this MSVC/x86 SSE path, so this call is compiled out there.
	 */
	_mm_setcsr(0x1f80u);
#else
	(void) 0;
#endif
}
