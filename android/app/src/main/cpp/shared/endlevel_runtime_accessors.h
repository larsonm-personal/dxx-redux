// Included by each engine after its private endlevel storage declarations
// This observer never resets state or consumes simulation/FX RNG
void endlevel_get_runtime_state(endlevel_runtime_state *state)
{
	state->sequence = Endlevel_sequence;
	state->data_loaded = endlevel_data_loaded;
	state->transition_segment = transition_segnum;
	state->exit_segment = exit_segnum;
	state->outside = outside_mine;
	state->explosion_playing = ext_expl_playing;
	state->mine_destroyed = mine_destroyed;
#ifdef DXX_BUILD_DESCENT_II
	state->movie_played = endlevel_movie_played;
#else
	state->movie_played = 0;
#endif
	state->current_speed = cur_fly_speed;
	state->desired_speed = desired_fly_speed;
	state->camera = endlevel_camera;
	state->exit_point = mine_exit_point;
	state->ground_exit_point = mine_ground_exit_point;
	state->side_exit_point = mine_side_exit_point;
	state->station_position = station_pos;
	state->exit_orientation = mine_exit_orient;
	state->surface_orientation = surface_orient;
	state->exit_angles = exit_angles;
	state->player_angles = player_angles;
	state->player_destination_angles = player_dest_angles;
	state->camera_angles = camera_cur_angles;
	state->camera_destination_angles = camera_desired_angles;
	state->frame = Endlevel_frame;
	state->fly[0] = fly_objects[0];
	state->fly[1] = fly_objects[1];
	state->explosion = external_explosion;
}
