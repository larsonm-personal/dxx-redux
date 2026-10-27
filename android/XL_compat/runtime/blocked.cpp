#include "blocked.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "gr.h"
}
namespace
{
unsigned map_calls = 0, projectile_entries = 0;
}
extern "C" int xl_blocked_entry(int index, int segment, int side)
{
	if (!xl_level_extensions_active() || index < 0 || index > Highest_object_index || segment < 0 || segment >= Num_segments || side < 0 || side >= 6) return 0;
	if (!xl_blocked_segment(Segments[segment].children[side])) return 0;
	const auto type = Objects[index].type;
	return type == OBJ_PLAYER || type == OBJ_ROBOT || type == OBJ_POWERUP;
}
extern "C" int xl_blocked_map_color(int segment, int fallback)
{
	if (!xl_blocked_map_segment(segment)) return fallback;
	++map_calls;
	return BM_XRGB(13, 13, 13);
}
extern "C" unsigned xl_blocked_map_calls(void)
{
	return map_calls;
}
extern "C" unsigned xl_blocked_projectile_entries(void)
{
	return projectile_entries;
}
extern "C" void xl_blocked_note_move(int index, int previous)
{
	if (index >= 0 && index <= Highest_object_index && Objects[index].type == OBJ_WEAPON && Objects[index].segnum != previous && xl_blocked_segment(Objects[index].segnum)) ++projectile_entries;
}
extern "C" void xl_blocked_reset(void)
{
	map_calls = projectile_entries = 0;
}
