#ifndef DXX_XL_ROBOT_ATTACK_MODE_H
#define DXX_XL_ROBOT_ATTACK_MODE_H
namespace xl_compat
{
constexpr int hostile_robot = -1, neutral_robot = 0, friendly_robot = 1;
inline bool robot_disarmed(int mode)
{
	return mode >= neutral_robot;
}
inline bool robot_attacks_player(int mode, bool robot_or_reactor, bool firing)
{
	return robot_or_reactor && firing && !robot_disarmed(mode);
}
} // namespace xl_compat
#endif
