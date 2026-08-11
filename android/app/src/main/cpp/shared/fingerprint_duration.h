#ifndef FINGERPRINT_DURATION_H
#define FINGERPRINT_DURATION_H

#include <stdint.h>

static inline int fingerprint_durations_compatible(int first_ms,
                                                   int second_ms,
                                                   float tolerance)
{
	if (first_ms <= 0 || second_ms <= 0)
		return 1;
	const int64_t first = first_ms;
	const int64_t second = second_ms;
	const int64_t difference = first > second ? first - second : second - first;
	const int64_t denominator = first > second ? first : second;
	return (double) difference <= (double) denominator * tolerance;
}

#endif
