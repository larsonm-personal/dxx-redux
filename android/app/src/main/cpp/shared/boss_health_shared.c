#include "boss_health_shared.h"

#include "game.h"
#include "gameseq.h"
#include "inferno.h"
#include "object.h"
#include "robot.h"
#ifdef __ANDROID__
#include "android_log.h"
#endif

int32_t boss_health_maximum_for_difficulty(int32_t strength, int difficulty)
{
#ifdef DXX_BUILD_DESCENT_II
	int32_t maximum = strength / (NDL + 3) * (difficulty + 4);

	if (difficulty == 0)
		maximum /= 2;
	return maximum;
#else
	(void) difficulty;
	return strength;
#endif
}

void difficulty_health_rescale_live_robots(int old_difficulty,
                                           int new_difficulty)
{
	int i;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *robot = &Objects[i];
		fix old_maximum, new_maximum, old_shields;
		const char *kind = "boss";

		if (robot->type != OBJ_ROBOT || robot->shields <= 0)
			continue;
		if (Robot_info[robot->id].boss_flag) {
			old_maximum = boss_health_maximum_for_difficulty(
			    Robot_info[robot->id].strength, old_difficulty);
			new_maximum = boss_health_maximum_for_difficulty(
			    Robot_info[robot->id].strength, new_difficulty);
#ifdef DXX_BUILD_DESCENT_II
		} else if (Robot_info[robot->id].thief ||
		           Robot_info[robot->id].companion) {
			kind = Robot_info[robot->id].companion ? "guidebot" : "thief";
			old_maximum = d2_thief_or_companion_health_maximum(
			    Robot_info[robot->id].strength, Current_level_num,
			    old_difficulty, Robot_info[robot->id].companion);
			new_maximum = d2_thief_or_companion_health_maximum(
			    Robot_info[robot->id].strength, Current_level_num,
			    new_difficulty, Robot_info[robot->id].companion);
#endif
		} else {
			continue;
		}
		old_shields = robot->shields;
		robot->shields = boss_health_rescale_value(
		    robot->shields, old_maximum, new_maximum);
#ifdef __ANDROID__
		debug_log(DLOG_GAME,
		          "difficulty robot shields: kind=%s obj=%d robot=%d old_diff=%d new_diff=%d old=%d new=%d old_max=%d new_max=%d",
		          kind, i, robot->id, old_difficulty, new_difficulty,
		          old_shields, robot->shields, old_maximum, new_maximum);
#endif
	}
}
