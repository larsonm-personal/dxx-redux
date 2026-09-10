#include <stdio.h>
#include "swept_edge_clearance.h"

static int check(const char *name, const double from[3], const double to[3],
                 const double start[3], const double end[3], double radius, int expected)
{
	const int actual = dxx_swept_sphere_misses_edge(from, to, start, end, radius);
	if (actual != expected) {
		fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
		return 0;
	}
	return 1;
}

int main(void)
{
	/* A ship fits through a 10-unit opening even when the offset plane of
	 * the funnel wall projects a contact onto the lip */
	const double from[3] = {0, 10, 0}, to[3] = {0, -5, 0};
	const double lip0[3] = {-5, 0, 5}, lip1[3] = {5, 0, 5};
	const double offset_from[3] = {0, 10, 1}, offset_to[3] = {0, -5, 1};
	const double short_to[3] = {0, 8, 0};
	const double point[3] = {0, 0, 5};
	const double parallel0[3] = {1, 10, 0}, parallel1[3] = {1, -5, 0};
	int ok = 1;
	ok &= check("funnel clearance", from, to, lip0, lip1, 4.735, 1);
	ok &= check("off-center edge impact", offset_from, offset_to, lip0, lip1, 4.735, 0);
	ok &= check("tangent contact", from, to, lip0, lip1, 5, 0);
	ok &= check("finite sweep", from, short_to, lip0, lip1, 6, 1);
	ok &= check("edge endpoint contact", from, to, point, point, 5, 0);
	ok &= check("stationary clearance", from, from, lip0, lip1, 4.735, 1);
	ok &= check("stationary overlap", point, point, lip0, lip1, 1, 0);
	ok &= check("parallel conservative result", from, to, parallel0, parallel1, 0.5, 0);
	/* Translation and fixed-point scale must not change the clearance result */
	for (int hit = 0; hit < 2; ++hit) {
		double a[3], b[3], c[3], d[3];
		for (int i = 0; i < 3; ++i) {
			a[i] = -12000000 + (hit ? offset_from[i] : from[i]) * 65536;
			b[i] = -12000000 + (hit ? offset_to[i] : to[i]) * 65536;
			c[i] = -12000000 + lip0[i] * 65536;
			d[i] = -12000000 + lip1[i] * 65536;
		}
		ok &= check("fixed coordinates", a, b, c, d, 310325 + 1, !hit);
	}
	return ok ? 0 : 1;
}
