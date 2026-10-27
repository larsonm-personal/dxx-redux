#ifndef XL_BOSS_ROUTES_H
#define XL_BOSS_ROUTES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include <vector>
namespace xl_compat
{
struct BossRoutes {
	int home = -1;
	std::vector<int> segments;
};
std::vector<BossRoutes> boss_routes();
// apply transfers already validated storage without allocating during publication
bool restore_boss_routes(std::vector<BossRoutes> &states, int apply);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
