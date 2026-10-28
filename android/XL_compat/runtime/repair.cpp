#include "repair.h"
#include "preview.h"
#include <limits>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "digi.h"
#include "sounds.h"
#include "gr.h"
}
namespace
{
fix64 last_cue = 0;
unsigned cues = 0, map_calls = 0;
} // namespace
extern "C" void xl_repair_clear(void)
{
	last_cue = 0;
	cues = map_calls = 0;
}
extern "C" int64_t xl_repair_elapsed(void)
{
	if (last_cue < 0 && GameTime64 > (std::numeric_limits<fix64>::max)() + last_cue) return (std::numeric_limits<fix64>::max)();
	if (last_cue > 0 && GameTime64 < (std::numeric_limits<fix64>::min)() + last_cue) return (std::numeric_limits<fix64>::min)();
	return GameTime64 - last_cue;
}
extern "C" int xl_repair_restore(int64_t elapsed, int apply)
{
	if ((elapsed > 0 && GameTime64 < (std::numeric_limits<fix64>::min)() + elapsed) ||
	    (elapsed < 0 && GameTime64 > (std::numeric_limits<fix64>::max)() + elapsed)) return 0;
	if (apply) last_cue = GameTime64 - elapsed;
	return 1;
}
// -1 selects Redux; XL uses the source default ship and shield-fill rate
extern "C" int32_t xl_repair_amount(int segment, int32_t maximum)
{
	if (!xl_level_extensions_active()) return -1;
	if (!xl_repair_segment(segment) || maximum <= 0 || Players[Player_num].shields < 0) return 0;
	fix amount = fixmul(FrameTime, i2f(11));
	if (amount > maximum) amount = maximum;
	if (last_cue > GameTime64) last_cue = 0;
	if (xl_repair_elapsed() > F1_0 / 4) {
		digi_play_sample(SOUND_REFUEL_STATION_GIVING_FUEL, F1_0 / 2);
		++cues;
		last_cue = GameTime64;
	}
	return amount;
}
extern "C" unsigned xl_repair_cues(void)
{
	return cues;
}
extern "C" int xl_repair_map_color(int segment, int fallback)
{
	if (!xl_repair_segment(segment)) return fallback;
	++map_calls;
	return BM_XRGB(0, 15, 31);
}
extern "C" unsigned xl_repair_map_calls(void)
{
	return map_calls;
}
