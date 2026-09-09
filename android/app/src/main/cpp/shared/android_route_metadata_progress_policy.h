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

#ifdef __cplusplus
extern "C" {
#endif

android_route_metadata_progress_update
android_route_metadata_progress_policy(
    int request_generation,
    int current_generation,
    int current_permille,
    int current_state,
    int requested_permille,
    int requested_state);

#ifdef __cplusplus
}
#endif

#endif
