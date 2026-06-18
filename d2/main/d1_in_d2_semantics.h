/*
 *
 * Small D1-in-D2 gameplay policy helpers.
 *
 */

#ifndef _D1_IN_D2_SEMANTICS_H
#define _D1_IN_D2_SEMANTICS_H

struct object;

int d1_in_d2_use_d2_resource_drop_suppression(const struct object *objp, int game_mode);
int d1_in_d2_use_d2_badass_robot_explosion(void);
int d1_in_d2_ai_visibility_turns_robot(int player_visibility);
int d1_in_d2_ai_may_turn_randomly_without_visibility(int game_mode);
int d1_in_d2_ai_nearby_fire_shortcut_active(int dist_to_last_fired_upon_player_pos, int threshold);

#endif /* _D1_IN_D2_SEMANTICS_H */
