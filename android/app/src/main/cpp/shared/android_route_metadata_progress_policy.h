#ifndef DXX_ANDROID_ROUTE_METADATA_PROGRESS_POLICY_H
#define DXX_ANDROID_ROUTE_METADATA_PROGRESS_POLICY_H

#define ANDROID_ROUTE_METADATA_CALCULATING 0
#define ANDROID_ROUTE_METADATA_USEFUL      1
#define ANDROID_ROUTE_METADATA_COMPLETE    2
#define ANDROID_ROUTE_METADATA_FAILED      3

typedef struct android_route_metadata_progress_update {
	int accepted;
	int permille;
	int state;
} android_route_metadata_progress_update;

static inline android_route_metadata_progress_update
android_route_metadata_progress_policy(
    int request_generation,
    int current_generation,
    int current_permille,
    int current_state,
    int requested_permille,
    int requested_state)
{
	android_route_metadata_progress_update result = {
		0, current_permille, current_state
	};

	if (request_generation != current_generation ||
	    requested_state < ANDROID_ROUTE_METADATA_CALCULATING ||
	    requested_state > ANDROID_ROUTE_METADATA_FAILED)
		return result;
	if (requested_permille < 0)
		requested_permille = 0;
	if (requested_permille > 1000)
		requested_permille = 1000;
	if (requested_state != ANDROID_ROUTE_METADATA_COMPLETE &&
	    requested_permille >= 1000)
		requested_permille = 999;
	result.accepted = 1;
	if (requested_permille > result.permille)
		result.permille = requested_permille;
	if (requested_state == ANDROID_ROUTE_METADATA_COMPLETE ||
	    (requested_state == ANDROID_ROUTE_METADATA_USEFUL &&
	     current_state == ANDROID_ROUTE_METADATA_CALCULATING) ||
	    (requested_state == ANDROID_ROUTE_METADATA_FAILED &&
	     current_state != ANDROID_ROUTE_METADATA_COMPLETE))
		result.state = requested_state;
	return result;
}

#endif
