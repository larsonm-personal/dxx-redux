#ifndef DXX_XL_MISSION_FLOW_H
#define DXX_XL_MISSION_FLOW_H
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
// CMissionManager::AdvanceLevel; secret D2 exits retain the existing destination
inline int mission_exit_destination(bool d1, int version, int current, int entry, std::int32_t value, int previous)
{
	const auto following = [](int level) {
		if (level == std::numeric_limits<int>::max()) throw std::runtime_error("XL mission ordinal overflow");
		return level + 1;
	};
	if (d1) return following(current < 0 ? entry : current);
	if (current <= 0) return previous;
	const int authored = value / 65536;
	return version > 20 && authored > 0 ? authored : following(current);
}

// FindNextLevel prefers unvisited exits, then the lowest destination ordinal
template <typename Range, typename State>
int mission_death_destination(int version, int current, const Range &triggers, State state)
{
	if (current == std::numeric_limits<int>::max()) throw std::runtime_error("XL mission ordinal overflow");
	const int following = current + 1;
	if (version <= 20) return following;
	int next = std::numeric_limits<int>::max(), best_state = std::numeric_limits<int>::max();
	for (const auto &action : triggers) {
		if (action.type != 3) continue;
		const int destination = action.value / 65536 > 0 ? action.value / 65536 : following;
		const int candidate_state = state(destination);
		if (candidate_state >= 0 && (candidate_state < best_state || (candidate_state == best_state && destination < next))) {
			best_state = candidate_state;
			next = destination;
			if (!next) break;
		}
	}
	return next < std::numeric_limits<int>::max() ? next : following;
}

// Callbacks retain DoExit's observable ordering, including rejected destinations
template <typename World>
bool operate_mission_exit(World &world, int player, int &flags, std::int32_t value)
{
	if (player != world.local_player() || (flags & 4)) return false;
	world.stop_audio();
	world.stop_boost(player);
	world.advance(value);
	if (world.d1()) {
		world.end_sequence();
		return true;
	}
	if (world.current() > 0) {
		if (world.version() > 20 && world.destination_state() < 0) return false;
		if (!(flags & 8)) flags |= 4;
		world.end_sequence();
		return true;
	}
	if (world.current() < 0) {
		if (world.shield() < 0 || world.dead()) return false;
		world.exit_secret();
		return true;
	}
	return false;
}
} // namespace xl_compat
#endif
