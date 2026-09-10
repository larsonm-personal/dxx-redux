#ifndef DXX_GUIDED_MISSILE_ROUTE_H
#define DXX_GUIDED_MISSILE_ROUTE_H

#define GUIDED_MISSILE_ROUTE_MAX_POINTS 32

#ifdef __cplusplus
extern "C" {
#endif

typedef struct guided_missile_route_point {
	int segment;
	int position[3];
} guided_missile_route_point;

typedef struct guided_missile_route {
	int wall;
	guided_missile_route_point launch;
	int point_count;
	guided_missile_route_point points[GUIDED_MISSILE_ROUTE_MAX_POINTS];
} guided_missile_route;

/* Predicts projectile geometry only; equipment and physical flight need separate verification */
int guided_missile_route_find(int wall, const int *launch_segments, int launch_count,
                              int player_radius, int (*consume_work)(void *), void *work_user,
                              guided_missile_route *result);

#ifdef __cplusplus
}
#endif
#endif
