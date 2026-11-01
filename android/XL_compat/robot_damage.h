#ifndef DXX_XL_ROBOT_DAMAGE_H
#define DXX_XL_ROBOT_DAMAGE_H
#include <cstdint>
namespace xl_compat
{
// Preconditions before boss bookkeeping and shield changes in ApplyDamageToRobot
inline bool robot_damage_allowed(bool definition, bool exploding, int shields, int strength,
                                 std::int64_t now, std::int64_t created, bool attacks_robots, bool killer_robot, bool killer_guidebot)
{
	return definition && !exploding && shields >= 0 && strength > 0 && now - created >= 65536 &&
	       (attacks_robots || !killer_robot || killer_guidebot);
}
} // namespace xl_compat
#endif
