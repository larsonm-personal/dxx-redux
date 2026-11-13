#include "input_demo_fp_env.h"
#include <fenv.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <xmmintrin.h>
#endif

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	char error[256] = "";
#if defined(INPUT_DEMO_FP_EXPECT_SUPPORTED) && INPUT_DEMO_FP_EXPECT_SUPPORTED
	if (!INPUT_DEMO_STRICT_FP_BUILD) return fail("Configured strict build did not expose its qualified C++ profile");
#endif
#ifdef INPUT_DEMO_FP_EXPECT_REJECT
	if (INPUT_DEMO_STRICT_FP_BUILD) return fail("Unsupported C++ compiler profile was admitted");
#endif
	if (!INPUT_DEMO_STRICT_FP_BUILD) {
		if (input_demo_validate_xl_fp_environment(error, sizeof(error)) || !strstr(error, "strict floating point build"))
			return fail("Unsupported C compiler profile was admitted");
		puts("PASS: unsupported C/C++ profile rejects XL input demos");
		return 0;
	}
	if (!input_demo_configure_startup_fp_environment(error, sizeof(error)) ||
	    !input_demo_validate_xl_fp_environment(error, sizeof(error))) return fail(error);
#if defined(_MSC_VER) && defined(_M_IX86)
	unsigned int original_precision = 0;
	if (_controlfp_s(&original_precision, 0, 0) != 0) return fail("Could not read x87 precision");
	for (const unsigned precision : { _PC_24, _PC_64 }) {
		unsigned int observed = 0;
		if (_controlfp_s(&observed, precision, _MCW_PC) != 0) return fail("Could not set negative x87 precision");
		const bool admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
		_controlfp_s(&observed, 0, 0);
		const bool changed = (observed & _MCW_PC) != precision;
		_controlfp_s(&observed, original_precision, _MCW_PC);
		if (admitted || changed) return fail("Invalid x87 precision was accepted or silently repaired");
	}
#endif
	const int original_rounding = fegetround();
	for (const int mode : { FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO }) {
		if (fesetround(mode) != 0) return fail("Could not set negative rounding control");
		const bool admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
		const bool changed = fegetround() != mode;
		if (fesetround(original_rounding) != 0) return fail("Could not restore test rounding mode");
		if (admitted || changed) return fail("Invalid rounding was accepted or silently repaired");
	}
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#if !defined(_MSC_VER)
	unsigned short x87_original = 0, x87_observed = 0;
	__asm__ volatile("fnstcw %0" : "=m"(x87_original));
	const unsigned short x87_unmasked = x87_original & ~1u;
	__asm__ volatile("fldcw %0" : : "m"(x87_unmasked));
	const bool x87_admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
	__asm__ volatile("fnstcw %0" : "=m"(x87_observed));
	__asm__ volatile("fldcw %0" : : "m"(x87_original));
	if (x87_admitted || x87_observed != x87_unmasked) return fail("Unmasked x87 exception was accepted or silently repaired");
#endif
	const unsigned original = _mm_getcsr();
	for (const unsigned mode : { original | 0x8000u, original | 0x40u, original & ~0x80u }) {
		_mm_setcsr(mode);
		const bool admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
		const bool changed = _mm_getcsr() != mode;
		_mm_setcsr(original);
		if (admitted || changed) return fail("Invalid SSE mode was accepted or silently repaired");
	}
	_mm_setcsr(original | 0x20u);
	const bool status_admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
	_mm_setcsr(original);
	if (!status_admitted) return fail("Accrued inexact status was treated as an FP control change");
#elif defined(__aarch64__)
	unsigned long original;
	__asm__ volatile("mrs %0, fpcr" : "=r"(original));
	const unsigned long flushed = original | (1ul << 24);
	__asm__ volatile("msr fpcr, %0" : : "r"(flushed));
	const bool admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
	unsigned long observed;
	__asm__ volatile("mrs %0, fpcr" : "=r"(observed));
	__asm__ volatile("msr fpcr, %0" : : "r"(original));
	if (admitted || observed != flushed) return fail("ARM flush mode was accepted or silently repaired");
#elif defined(__arm__) && defined(__ARM_FP)
	unsigned original;
	__asm__ volatile("vmrs %0, fpscr" : "=r"(original));
	const unsigned flushed = original | (1u << 24);
	__asm__ volatile("vmsr fpscr, %0" : : "r"(flushed));
	const bool admitted = input_demo_validate_xl_fp_environment(error, sizeof(error)) != 0;
	unsigned observed;
	__asm__ volatile("vmrs %0, fpscr" : "=r"(observed));
	__asm__ volatile("vmsr fpscr, %0" : : "r"(original));
	if (admitted || observed != flushed) return fail("ARM flush mode was accepted or silently repaired");
#endif
	if (!input_demo_validate_xl_fp_environment(error, sizeof(error))) return fail(error);
	puts("PASS: native FP admission, altered modes reject without repair, original mode restored");
	return 0;
}
