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
int input_demo_replay_collision_probe_active(void);
int input_demo_trace_collision_pose_active(void);
unsigned int input_demo_trace_collision_frame_index(void);
const char *input_demo_trace_collision_mode_name(void);
void input_demo_record_wall_impact_event(struct object *weapon, int hitwall, int blew_up, int robot_escort);
void input_demo_record_robot_damage_event(struct object *robot, int32_t damage, int32_t old_shields);
void input_demo_record_robot_impact_event(struct object *weapon, struct object *robot);
void input_demo_record_player_damage_event(int32_t damage, int32_t old_shields, struct object *killer, int possibly_friendly);
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
void input_demo_consume_awareness_source(const char **source_tag, int *source_objnum, int *aux_objnum);
void input_demo_set_awareness_source(const char *source_tag, int source_objnum, int aux_objnum);
void input_demo_trace_tracked_robot_poses(void);
int input_demo_trace_ai_active(void);
int input_demo_replay_awareness_probe_active(void);
int input_demo_replay_homing_desync_probe_active(void);
int input_demo_homing_desync_probe_active(void);
int input_demo_trace_escort_active(void);
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
int input_demo_robot_lifecycle_probe_active(void);
int input_demo_robot_visual_probe_active(void);
int input_demo_robot_lifecycle_is_target(int objnum, struct object *obj);
int input_demo_should_match_android_companion_velocity(void);
unsigned int input_demo_trace_frame_index(void);
int input_demo_replay_path_probe_active(struct object *objp);
int input_demo_replay_follow_probe_active(struct object *objp);
int input_demo_replay_path_request_probe_active(struct object *objp);
void input_demo_reset_escort_state_probes(void);
void input_demo_log_escort_rng_progress(const char *label, unsigned int *rng_before, unsigned int *rng_call_count_before);
void input_demo_log_escort_path_state(const char *label, struct object *objp);
void input_demo_log_escort_restore_normalization(struct object *objp, struct ai_local *ailp, int64_t raw_time_player_seen, int64_t raw_escort_last_path_created);
void input_demo_log_escort_segment_change(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int player_seg, int believed_seg);
void input_demo_log_escort_visit_change(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int player_seg, int believed_seg, int away_gate, int recent_path_gate, int goto_player_gate, int same_seg_gate, int early_path_gate, int visit);
void input_demo_log_escort_state(struct object *objp, struct ai_local *ailp, struct ai_static *aip, int32_t dist_to_player, int player_visibility, int should_visit_player, int64_t since_seen, int64_t since_player_path, int away_gate, int recent_path_gate, int goto_player_gate, int same_seg_gate, int early_path_gate);
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