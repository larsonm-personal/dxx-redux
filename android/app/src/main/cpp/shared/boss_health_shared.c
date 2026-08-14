#include "boss_health_shared.h"

#include "game.h"
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

void boss_health_rescale_live_bosses(int old_difficulty, int new_difficulty)
{
	int i;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *boss = &Objects[i];
		fix old_maximum, new_maximum, old_shields;

		if (boss->type != OBJ_ROBOT || boss->shields <= 0 ||
		    !Robot_info[boss->id].boss_flag)
			continue;
		old_maximum = boss_health_maximum_for_difficulty(
		    Robot_info[boss->id].strength, old_difficulty);
		new_maximum = boss_health_maximum_for_difficulty(
		    Robot_info[boss->id].strength, new_difficulty);
		old_shields = boss->shields;
		boss->shields = boss_health_rescale_value(
		    boss->shields, old_maximum, new_maximum);
#ifdef __ANDROID__
		debug_log(DLOG_GAME,
		          "difficulty boss shields: obj=%d robot=%d old_diff=%d new_diff=%d old=%d new=%d old_max=%d new_max=%d",
		          i, boss->id, old_difficulty, new_difficulty, old_shields,
		          boss->shields, old_maximum, new_maximum);
#endif
	}
}
