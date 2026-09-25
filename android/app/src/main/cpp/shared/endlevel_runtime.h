#ifndef DXX_ENDLEVEL_RUNTIME_H
#define DXX_ENDLEVEL_RUNTIME_H

#include "object.h"

// Shared observation contract; engine pointers become object slots in traces
typedef struct flythrough_data {
	object *obj;
	vms_angvec angles;      // orientation in angles
	vms_vector step;        // how far in a second
	vms_vector angstep;     // rotation per second
	fix speed;              // how fast object is moving
	vms_vector headvec;     // where we want to be pointing
	int first_time;         // flag for if first time through
	int transition_reached; // remember a camera transition crossed within a slow frame
	fix offset_frac;        // how far off-center as portion of way
	fix offset_dist;        // how far currently off-center
} flythrough_data;

typedef struct endlevel_frame_state {
	fix timer, bank_rate, explosion_wait1, explosion_wait2, ext_expl_halflife;
	int sound_count;
} endlevel_frame_state;

typedef struct endlevel_runtime_state {
	int sequence, data_loaded, transition_segment, exit_segment, outside, explosion_playing, mine_destroyed;
	int movie_played;
	fix current_speed, desired_speed;
	object *camera;
	vms_vector exit_point, ground_exit_point, side_exit_point, station_position;
	vms_matrix exit_orientation, surface_orientation;
	vms_angvec exit_angles, player_angles, player_destination_angles, camera_angles, camera_destination_angles;
	endlevel_frame_state frame;
	flythrough_data fly[2];
	object explosion;
} endlevel_runtime_state;

#ifdef __cplusplus
extern "C" {
#endif
void endlevel_get_runtime_state(endlevel_runtime_state *state);
#ifdef __cplusplus
}
#endif
#endif
