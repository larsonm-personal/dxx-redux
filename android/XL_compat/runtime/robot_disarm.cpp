#include "robot_disarm.h"
#include "robot_disarm_state.h"
#include "../robot_attack_mode.h"
#include "preview.h"
#include <map>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "switch.h"
}
namespace
{
std::map<int, int> neutral;
bool bound(int index, int signature)
{
	return index >= 0 && index <= Highest_object_index && Objects[index].type == OBJ_ROBOT && Objects[index].signature == signature;
}
} // namespace
extern "C" void xl_disarm_clear(void)
{
	neutral.clear();
}
extern "C" int xl_robot_disarmed(int index)
{
	if (!xl_level_extensions_active()) return 0;
	const auto found = neutral.find(index);
	const int mode = found != neutral.end() && bound(found->first, found->second) ? xl_compat::neutral_robot : xl_compat::hostile_robot;
	return xl_compat::robot_disarmed(mode);
}
extern "C" int xl_disarm_action_valid(const trigger *action)
{
	if (!action || action->type != 34 || action->num_links < 0 || action->num_links > MAX_WALLS_PER_LINK) return 0;
	return 1;
}
extern "C" int xl_disarm_action(const trigger *action)
{
	if (!xl_level_extensions_active() || !xl_disarm_action_valid(action)) return 0;
	for (int i = 0; i < action->num_links; ++i) {
		const int index = action->seg[i];
		if (action->side[i] < 0 && index >= 0 && index <= Highest_object_index && Objects[index].type == OBJ_ROBOT)
			neutral[index] = Objects[index].signature;
	}
	return 1;
}
namespace xl_compat
{
nlohmann::json disarm_snapshot()
{
	auto result = nlohmann::json::array();
	for (const auto &entry : neutral)
		if (bound(entry.first, entry.second)) result.push_back({ entry.first, entry.second });
	return result;
}
bool disarm_restore(const nlohmann::json &state, bool apply)
{
	if (!xl_level_extensions_active() || !state.is_array() || state.size() > std::size_t(object_capacity())) return false;
	std::map<int, int> prepared;
	int previous = -1;
	for (const auto &row : state) {
		if (!row.is_array() || row.size() != 2) return false;
		for (const auto &field : row)
			if (!field.is_number_integer() || (field.is_number_unsigned() && field.get<std::uint64_t>() > INT32_MAX) || field.get<std::int64_t>() < INT32_MIN || field.get<std::int64_t>() > INT32_MAX) return false;
		const int index = row[0].get<int>(), signature = row[1].get<int>();
		if (index <= previous || !bound(index, signature)) return false;
		prepared.emplace(index, signature);
		previous = index;
	}
	if (apply) neutral = std::move(prepared);
	return true;
}
} // namespace xl_compat
