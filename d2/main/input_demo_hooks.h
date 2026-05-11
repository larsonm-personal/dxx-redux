#ifndef D2X_INPUT_DEMO_HOOKS_H
#define D2X_INPUT_DEMO_HOOKS_H

#include <stdint.h>

struct ai_local;
struct ai_static;
struct g3s_lrgb;
struct g3s_point;
struct object;
struct vms_vector;

typedef struct input_demo_result input_demo_result;
typedef struct input_demo_state_trace_diag input_demo_state_trace_diag;

#ifdef __cplusplus
extern "C" {
#endif

void input_demo_record_game_frame(void);
void input_demo_update_rng_trace_context(void);
void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
void input_demo_set_recording_terminal_exit(int terminal_exit);
void input_demo_clear_recording_terminal_exit(void);
int input_demo_replay_collision_probe_active(void);
int input_demo_trace_collision_pose_active(void);
unsigned int input_demo_trace_collision_frame_index(void);
const char *input_demo_trace_collision_mode_name(void);
void input_demo_record_wall_impact_event(struct object *weapon, int hitwall, int blew_up, int robot_escort);
void input_demo_record_robot_damage_event(struct object *robot, int32_t damage, int32_t old_shields);
void input_demo_record_robot_impact_event(struct object *weapon, struct object *robot);
void input_demo_record_player_damage_event(int32_t damage, int32_t old_shields, struct object *killer, int possibly_friendly);
void input_demo_log_weapon_robot_fvi_check(struct object *weapon, struct object *robot,
	const struct vms_vector *p0, const struct vms_vector *p1,
	int32_t fudged_rad, int32_t combined_rad, int32_t center_dist,
	int32_t miss_delta, int32_t d);
int input_demo_replay_powerup_probe_active(void);
void input_demo_log_reactor_hit(struct object *controlcen, int32_t damage, int32_t old_shields);
void input_demo_log_reactor_destroyed(struct object *controlcen);
void input_demo_log_score_probe(const char *score_kind, int points, int score_after);
void input_demo_log_collision_player_wall_impact(const char *mode_name, unsigned int frame_index, struct object *playerobj, int hitseg, int hitwall, int hitspeed, int damage, int force_field_hit);
void input_demo_log_replay_player_wall_collision(struct object *playerobj, int hitseg, int hitwall, int hitspeed, const struct vms_vector *hitpt);
void input_demo_log_collision_weapon_wall_impact(const char *mode_name, unsigned int frame_index, struct object *weapon, int hitwall, int blew_up, int robot_escort);
void input_demo_log_collision_player_robot_impact(const char *mode_name, unsigned int frame_index, struct object *playerobj, struct object *robot);
void input_demo_log_collision_weapon_robot_impact(const char *mode_name, unsigned int frame_index, struct object *weapon, struct object *robot);
void input_demo_log_replay_robot_damage(struct object *robot, int32_t damage, int32_t old_shields);
void input_demo_trace_player_shield_change(const char *cause, int32_t shields_before, int32_t shields_after, const char *extra_json, const char *extra_log);
void input_demo_log_player_damage_probe(const char *mode_name, unsigned int frame_index, int damage, int32_t old_shields, int32_t shields_after, int killer_type, int killer_obj, int killer_id, int killer_sig, int killer_seg, int possibly_friendly);
void input_demo_log_player_weapon_hit(const char *mode_name, unsigned int frame_index, struct object *weapon, struct object *playerobj, int damage, const struct vms_vector *collision_point);
void input_demo_log_replay_powerup_probe_before(struct object *powerup, int energy_before, int shields_before);
void input_demo_log_replay_powerup_probe_after(struct object *powerup, int powerup_used, int energy_before, int energy_after, int shields_before, int shields_after);
void input_demo_log_replay_object_object_collision(struct object *a, struct object *b, const struct vms_vector *collision_point);
void input_demo_log_player_bump_probe(const char *step, struct object *obj0, struct object *obj1,
	const struct vms_vector *relative_velocity, const struct vms_vector *float_force,
	int32_t scale_num, int32_t scale_den, int damage_flag);
int input_demo_replay_spreadfire_probe_active(void);
int input_demo_replay_weapon_focus_active(void);
int input_demo_weapon_trace_active(void);
int input_demo_weapon_create_probe_active(struct object *obj);
int input_demo_replay_is_player_owned_weapon(struct object *obj);
void input_demo_maybe_track_suspect_spreadfire(struct object *obj);
void input_demo_log_weapon_lifetime(const char *step, struct object *obj);
void input_demo_record_homing_state(const char *step, struct object *obj,
	int straight_time_active, int do_homer_frame, int track_goal_before,
	int track_goal_after, int dot, int32_t ideal_homer_frame_time,
	unsigned int homer_frame_count);
void input_demo_record_weapon_create_event(struct object *obj);
void input_demo_record_player_shot_event(struct object *obj, int laser_type, int gun_num, int32_t spreadr, int32_t spreadu, int32_t delay_time, int make_sound, int harmless);
void input_demo_record_spreadfire_emit_event(int nfires, int flags, int spreadfire_toggle, int64_t next_laser_delta, int64_t last_laser_delta);
void input_demo_log_replay_player_shot_probe(struct object *obj, int laser_type,
	int gun_num, int32_t spreadr, int32_t spreadu, int32_t delay_time,
	int make_sound, int harmless, const vms_vector *shot_orientation);
void input_demo_log_replay_spreadfire_emit_probe(int nfires, int flags,
	int spreadfire_toggle, int64_t next_laser_delta,
	int64_t last_laser_delta);
void input_demo_log_replay_fusion_warmup_probe(struct object *playerobj,
	int auto_fire_active, int32_t fusion_charge, int64_t next_sound_time);
void input_demo_consume_awareness_source(const char **source_tag, int *source_objnum, int *aux_objnum);
void input_demo_set_awareness_source(const char *source_tag, int source_objnum, int aux_objnum);
void input_demo_trace_tracked_robot_poses(void);
int input_demo_trace_ai_active(void);
int input_demo_replay_awareness_probe_active(void);
int input_demo_replay_homing_desync_probe_active(void);
int input_demo_replay_obj95_state_probe_active(struct object *obj);
int input_demo_homing_desync_probe_active(void);
int input_demo_trace_escort_active(void);
int input_demo_trace_ai_visibility_active(struct object *objp);
void input_demo_log_ai_visibility_probe(struct object *objp,
	const char *step_label, int previous_visibility,
	int raw_player_visibility, int final_player_visibility,
	int sight_sound_gate, int attack_sound_gate, int misc_sound_gate,
	const struct vms_vector *pos,
	const struct vms_vector *believed_player_pos);
void input_demo_log_ai_awareness_roll_probe(struct object *objp,
	const char *step_label, int previous_visibility, int player_visibility,
	int dist_to_player, int obj_ref, int roll, int threshold, int pass,
	int headlight);
int input_demo_trace_ai_robot_active(struct object *objp, struct ai_static *aip, struct ai_local *ailp);
void input_demo_log_ai_robot_state(const char *label, struct object *objp);
void input_demo_note_ai_schedule_skip_return(struct object *objp);
void input_demo_note_ai_schedule_timeslice_return(struct object *objp);
void input_demo_note_ai_schedule_process(struct object *objp);
void input_demo_note_ai_schedule_phys_skip(struct object *objp, int skip_before, int skip_after);
void input_demo_note_ai_schedule_detail(const char *kind, struct object *objp,
	int previous_visibility, int awareness_type, int awareness_time,
	int skip_ai_count, int time_since_processed, int dist_to_player,
	int obj_ref);
void input_demo_log_ai_schedule_record_probe(const char *step_label,
	struct object *objp, struct ai_static *aip, struct ai_local *ailp,
	int previous_visibility, int dist_to_player, int obj_ref);
void input_demo_append_replay_probe_message(const char *kind, struct object *objp,
	const char *message);
void input_demo_log_awareness_vulcan_roll(struct object *objp, int type, int vulcan_roll, unsigned int sim_calls_before, unsigned int sim_state_before);
void input_demo_log_awareness_add_return(const char *reason, struct object *objp, int type, int added, unsigned int sim_calls_before, unsigned int sim_calls_after, unsigned int sim_state_before, unsigned int sim_state_after);
void input_demo_log_awareness_entry(struct object *objp, int type, const char *source_tag, int source_objnum, int aux_objnum, unsigned int sim_calls_entry, unsigned int sim_state_entry, int num_awareness_before, int overall_agitation_before, int multiplayer_awareness_allowed);
void input_demo_log_awareness_probe(struct object *objp, int type);
void input_demo_log_awareness_post_add(struct object *objp, int type, int awareness_added, unsigned int sim_calls_entry, unsigned int sim_calls_after_add, unsigned int sim_state_entry, unsigned int sim_state_after_add);
void input_demo_log_awareness_post_gate(struct object *objp, int type, int rng_gate_value, int rng_gate_pass, unsigned int sim_calls_before, unsigned int sim_calls_after, unsigned int sim_state_before, unsigned int sim_state_after);
void input_demo_log_awareness_result(struct object *objp, int type, const char *source_tag, int source_objnum, int aux_objnum, int awareness_added, int skipped_observer, int awareness_before, int awareness_after, int agitation_before, int agitation_after, int multiplayer_awareness_allowed, int rng_gate_value, int rng_gate_pass);
void input_demo_log_ai_state(void);
void input_demo_log_ai_frame(void);
void input_demo_log_ai_frame_summary(int traced_robot_count);
int input_demo_trace_ai_rng_active(struct object *obj);
void input_demo_log_ai_fire_gate_probe(struct object *obj,
	const char *step_label, int fire_gun, int player_visibility,
	int dist_to_player, int dot, int dot_threshold, int roll,
	int roll_threshold, int melee_limit, int hit_dist,
	int object_animates);
void input_demo_log_ai_fire_probe(struct object *obj, const char *step_label,
	int fire_gun, int player_visibility, int dist_to_player);
void input_demo_log_ai_rng_probe(struct object *obj, unsigned int rng_before,
	unsigned int rng_call_count_before, unsigned int rng_after,
	unsigned int rng_call_count_after);
void input_demo_log_ai_agitation_path_gate(struct object *objp,
	int dist_to_player, int overall_agitation, int trigger_roll,
	int trigger_threshold, int trigger_pass, int path_roll, int path_scale,
	int path_pass, int max_length, int pre_mode, int pre_goal_segment,
	int pre_path_index, int pre_path_length, int pre_hide_index,
	int pre_path_dir, int64_t pre_time_player_seen);
void input_demo_log_ai_chase_path_gate(struct object *objp,
	int dist_to_player, int previous_visibility, int player_visibility,
	int chase_gate_pass, int awareness_allowed, int path_created,
	int pre_mode, int pre_goal_segment, int pre_path_index,
	int pre_path_length, int pre_hide_index, int pre_path_dir,
	int64_t pre_time_player_seen);
void input_demo_log_ai_follow_path_transition(struct object *objp,
	int dist_to_player, int anger_level, int previous_visibility,
	int player_visibility, int awareness_allowed, int follow_called,
	int visible_chase_pass, int still_pass, int pre_mode,
	int pre_goal_segment, int pre_path_index, int pre_path_length,
	int pre_hide_index, int pre_path_dir, int64_t pre_time_player_seen);
void input_demo_log_motion_fix_illegal_before(struct object *obj, int frame,
	int hitseg, int hitside, int hitface,
	const struct vms_vector *origin);
void input_demo_log_motion_fix_illegal_after(struct object *obj, int frame);
void input_demo_log_preserved_ui_rng_probe(const char *stage,
	int preserve_rng, unsigned int saved_rng_state,
	unsigned int current_rng_state, int cockpit_mode, int no_draw_hud,
	int observer);
void input_demo_log_checkpoint_runtime_restore(int64_t game_time,
	int64_t next_laser_delta, int64_t next_missile_delta,
	int64_t last_laser_delta, int64_t next_flare_delta,
	int64_t auto_fusion_delta, int global_laser_firing_count,
	int global_missile_firing_count, int spreadfire_toggle,
	int missile_gun, int helix_orientation, int proximity_dropped,
	int smartmines_dropped, int64_t omega_delta, int d_tick_count,
	int d_tick_step, int d_tick_timer, unsigned int rng_state,
	int has_rng_state);
int input_demo_robot_lifecycle_probe_active(void);
int input_demo_robot_visual_probe_active(void);
int input_demo_robot_lifecycle_is_target(int objnum, struct object *obj);
int input_demo_should_match_android_companion_velocity(void);
unsigned int input_demo_trace_frame_index(void);
int input_demo_replay_path_probe_active(struct object *objp);
int input_demo_replay_follow_probe_active(struct object *objp);
int input_demo_replay_path_request_probe_active(struct object *objp);
void input_demo_log_follow_advance_trigger(struct object *objp,
	int dist_to_goal, int threshold_distance, int velocity_mag);
void input_demo_log_follow_wrap(struct object *objp, int player_visibility);
void input_demo_log_follow_advance_result(struct object *objp,
	int original_index, int original_dir, int dist_to_goal,
	int threshold_distance, int velocity_mag, int forced_break);
void input_demo_reset_escort_state_probes(void);
void input_demo_log_escort_goal_probe(const char *step, struct object *objp,
	struct ai_local *ailp, struct ai_static *aip, int goal_seg,
	int goal_index);
void input_demo_log_escort_rng_progress(const char *label, unsigned int *rng_before, unsigned int *rng_call_count_before);
void input_demo_log_escort_path_state(const char *label, struct object *objp);
void input_demo_log_escort_restore_checkpoint(struct object *objp,
	struct ai_local *ailp, int have_checkpoint_thief_state,
	int buddy_messages_suppressed, int64_t buddy_sorry_time,
	int looking_for_marker, int last_buddy_key,
	int64_t last_buddy_message_time, int64_t last_come_back_message_time,
	int64_t buddy_last_missile_time, int64_t re_init_thief_time,
	int64_t last_thief_hit_time);
void input_demo_log_escort_restore_normalization(struct object *objp, struct ai_local *ailp, int64_t raw_time_player_seen, int64_t raw_escort_last_path_created);
void input_demo_log_escort_restore_state(struct object *objp,
	struct ai_local *ailp);
void input_demo_log_escort_segment_change(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int player_seg, int believed_seg);
void input_demo_log_escort_visit_change(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int player_seg, int believed_seg, int away_gate, int recent_path_gate, int goto_player_gate, int same_seg_gate, int early_path_gate, int visit);
void input_demo_log_escort_state(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int32_t dist_to_player, int player_visibility, int should_visit_player, int64_t since_seen, int64_t since_player_path, int away_gate, int recent_path_gate, int goto_player_gate, int same_seg_gate, int early_path_gate);
void input_demo_log_escort_goal_reset(void);
void input_demo_log_snipe_detail_probe(int entry_probe, const char *step, struct object *objp, struct ai_local *ailp, int player_visibility, int32_t dist_to_player);
void input_demo_log_thief_detail_probe(int entry_probe, const char *step, struct object *objp, struct ai_local *ailp, int player_visibility, int32_t dist_to_player);
void input_demo_log_path_robot_state(const char *label, struct object *objp);
void input_demo_log_path_request(const char *label, struct object *objp, int start_seg, int end_seg, int max_length, int safety_flag, int avoid_seg);
void input_demo_log_path_probe(struct object *objp, int start_seg, int end_seg, int max_depth, int random_flag, int safety_flag, int avoid_seg, int result, unsigned int rng_before, unsigned int rng_call_count_before);
void input_demo_log_path_detail(struct object *objp, int start_seg, int end_seg, int random_flag, int random_xlate_seed_count, int random_xlate_refresh_roll_count, int random_xlate_refresh_count, int queue_push_count, int raw_num_points, int final_num_points);
void input_demo_log_path_points(const char *label, struct object *objp, const void *psegs, int num_points);
void input_demo_log_robot_lifecycle_delete(int objnum, struct object *obj);
void input_demo_log_robot_visual_id38(struct object *obj, unsigned char probe_codes, int probe_behind, int probe_projected, const struct g3s_point *probe_point);
void input_demo_log_robot_visual_state_default(struct object *obj, const struct g3s_lrgb *light, unsigned char probe_codes, int probe_behind, int probe_projected, const struct g3s_point *probe_point);
void input_demo_log_robot_visual_state_tmap_override(struct object *obj, const struct g3s_lrgb *light, int override_bm_index, int override_bm_flags, unsigned char probe_codes, int probe_behind, int probe_projected, const struct g3s_point *probe_point);
void input_demo_log_robot_visual_player_cloak(struct object *obj);
void input_demo_log_robot_visual_robot_cloak(struct object *obj);
void input_demo_log_robot_poly_probe(struct object *obj, int faces_considered, int faces_drawn, int tmap_override);
unsigned int input_demo_trace_robot_fire_frame_index(void);
int input_demo_trace_robot_fire_active(struct object *objp);
void input_demo_log_robot_fire_probe(struct object *objp,
	const struct vms_vector *fire_vec, int weapon_type);
void input_demo_log_robot_fire_state(const char *label, struct object *objp,
	int weapon_type);
void input_demo_record_phys_apply_rot_event(struct object *obj,
	const struct vms_vector *force_vec, int skip_before, int addval,
	int skip_after, int rate, int vecmag, int tval);
void input_demo_record_claw_contact_event(const char *step, struct object *robot,
	struct object *playerobj, const struct vms_vector *collision_point,
	int next_fire, int distance_to_player, int damage);
void input_demo_update_result_kills_baseline(void);
void input_demo_log_current_replay_frame_state_mismatch(void);
void input_demo_log_replay_energy_stage(const char *label);
int input_demo_prepare_replay_frame(void);
int input_demo_step_replay_frame(void);
void input_demo_advance_replay_frame(void);
void input_demo_finish_replay_without_close(void);
int input_demo_finish_replay_from_level_exit(void);
int input_demo_finish_replay_from_mine_exit(void);

#ifdef __cplusplus
}
#endif

#endif