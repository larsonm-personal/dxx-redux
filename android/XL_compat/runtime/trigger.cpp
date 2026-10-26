#include "preview.h"
#include <array>
#include <cstdint>
#include <cstdlib>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
#include "timer.h"
}

namespace
{
int depth = -1;
std::uint64_t exit_generation = 0;
std::array<fix64, MAX_TRIGGERS> last_activation{};
std::array<bool, MAX_TRIGGERS> activated{};
struct TriggerDepth {
	TriggerDepth()
	{
		++depth;
	}
	~TriggerDepth()
	{
		--depth;
	}
};
int linked_trigger(const trigger &source, int link)
{
	const int segment = source.seg[link], side = source.side[link];
	if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6) return -1;
	const int wall = Segments[segment].sides[side].wall_num;
	if (wall < 0 || wall >= Num_walls) return -1;
	const int target = Walls[wall].trigger;
	return target >= 0 && target < Num_triggers ? target : -1;
}
} // namespace

extern "C" void xl_wall_trigger_reset(void)
{
	activated.fill(false);
}

extern "C" void xl_wall_trigger_exit(void)
{
	if (depth >= 0) ++exit_generation;
}

// Return -1 for ordinary engine dispatch; children retain the original actor
extern "C" int xl_wall_trigger(int index, int object_index, int shot)
{
	if (!xl_level_extensions_active() || index < 0 || index >= Num_triggers) return -1;
	if (depth > 15) return 1;
	auto &action = Triggers[index];
	const bool chain = action.type >= 31 && action.type <= 33;
	if (!chain) return xl_wall_teleport(index, object_index, shot);
	if ((Game_mode & GM_MULTI) || (action.flags & TF_DISABLED) || index >= MAX_TRIGGERS || object_index < 0 || object_index > Highest_object_index) return 1;
	const int actor = Objects[object_index].type;
	const bool player = actor == OBJ_PLAYER;
	if (player) {
		if (object_index != Players[Player_num].objnum) return 1;
	} else if (action.type != 31 || !xl_wall_teleport_actor(object_index)) return 1;
	const auto delay = std::abs(std::int64_t(action.time));
	if ((action.flags & ~7) || (delay >= 100 && delay <= 900000) || action.num_links < 0 || action.num_links > MAX_WALLS_PER_LINK) return 1;
	for (int i = 0; i < action.num_links; ++i)
		if (action.seg[i] < 0 || action.seg[i] > Highest_segment_index || action.side[i] < 0 || action.side[i] >= 6) return 1;
	TriggerDepth scope;
	const auto entry_exit = exit_generation;
	if (!depth) {
		const auto now = timer_query();
		if (activated[index] && now - last_activation[index] < F1_0 * 3 / 4) return 1;
		activated[index] = true;
		last_activation[index] = now;
	}
	for (int i = 0; i < action.num_links; ++i) {
		const int target_index = linked_trigger(action, i);
		if (target_index < 0) continue;
		auto &target = Triggers[target_index];
		if (action.type == 31) {
			if (player) check_trigger_sub(target_index, Player_num, 0);
			else xl_wall_trigger(target_index, object_index, 0);
			if (exit_generation != entry_exit) return 1;
		} else if (action.type == 32) {
			if (target.type == 31 && target.value > 0) --target.value;
			if (target.type != 31 || target.value <= 0) target.flags &= ~TF_DISABLED;
		} else {
			target.flags |= TF_DISABLED;
			if (target.type == 31) target.value = static_cast<std::int32_t>(std::uint32_t(target.value) + 1u);
		}
	}
	if (action.flags & TF_ONE_SHOT) action.flags |= TF_DISABLED;
	xl_preview_trigger_result(index, shot, 0);
	return 0;
}
