#include "swept_edge_clearance.h"

static double clamp_unit(double value)
{
	return value < 0 ? 0 : value > 1 ? 1
	                                 : value;
}

/* Closest points on two finite segments, including degenerate segments */
int dxx_swept_sphere_misses_edge(const double from[3], const double to[3],
                                 const double edge_start[3], const double edge_end[3], double radius)
{
	double u[3], v[3], w[3];
	double a = 0, b = 0, c = 0, d = 0, e = 0;
	double s = 0, t = 0, distance_squared = 0;
	for (int i = 0; i < 3; ++i) {
		u[i] = to[i] - from[i];
		v[i] = edge_end[i] - edge_start[i];
		w[i] = from[i] - edge_start[i];
		a += u[i] * u[i];
		b += u[i] * v[i];
		c += v[i] * v[i];
		d += u[i] * w[i];
		e += v[i] * w[i];
	}
	if (a == 0)
		t = c > 0 ? clamp_unit(e / c) : 0;
	else if (c == 0)
		s = clamp_unit(-d / a);
	else {
		const double determinant = a * c - b * b;
		/* Keep the existing collision result when nearly parallel lines make
		 * the closest-point calculation ill-conditioned */
		if (determinant <= a * c * 1e-12)
			return 0;
		s = clamp_unit((b * e - c * d) / determinant);
		t = (b * s + e) / c;
		if (t < 0) {
			t = 0;
			s = clamp_unit(-d / a);
		} else if (t > 1) {
			t = 1;
			s = clamp_unit((b - d) / a);
		}
	}
	for (int i = 0; i < 3; ++i) {
		const double separation = w[i] + s * u[i] - t * v[i];
		distance_squared += separation * separation;
	}
	return distance_squared > radius * radius;
}
