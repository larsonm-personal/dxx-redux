#ifndef CLASSIC_DEMO_JSON_D2_SNAPSHOT_H
#define CLASSIC_DEMO_JSON_D2_SNAPSHOT_H

#include "classic_demo_json.h"

struct object;

void classic_demo_json_d2_snapshot_header(classic_demo_json_header *snapshot,
                                          int version, int game_type);
void classic_demo_json_d2_snapshot_robot_damage(
    classic_demo_json_robot_damage *snapshot,
    int frame_number, int game_time, const struct object *robot,
    int old_shields, int damage);
int classic_demo_json_d2_snapshot_frame(classic_demo_json_frame *snapshot,
                                        classic_demo_json_object *objects, size_t object_capacity,
                                        int frame_number, int frame_time, int game_time,
                                        const classic_demo_json_control *control,
                                        const classic_demo_json_wiggle *wiggle);

#endif
