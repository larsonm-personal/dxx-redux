#include "preview.h"
#include "../trigger_flags.h"
#include "../texture_change.h"
extern "C" {
#include "inferno.h"
#include "switch.h"
#include "segment.h"
#include "wall.h"
#include "gameseg.h"
#include "digi.h"
#include "sounds.h"
#include "textures.h"
#include "hudmsg.h"
#include "game.h"
#ifdef __ANDROID__
#include "guidebot_extensions.h"
#endif
}

extern "C" int xl_trigger_texture_valid(const trigger *action)
{
	if (!action || action->type != 19 || action->num_links < 0 || action->num_links > MAX_WALLS_PER_LINK) return 0;
	const auto change = xl_compat::texture_change(action->value);
	if (action->num_links && (change.base >= NumTextures || change.overlay >= NumTextures)) return 0;
	for (int link = 0; link < action->num_links; ++link)
		if (action->seg[link] < 0 || action->seg[link] > Highest_segment_index || action->side[link] < 0 || action->side[link] >= 6) return 0;
	return 1;
}

extern "C" int xl_trigger_texture_action(const trigger *action)
{
	if (!xl_level_extensions_active() || (Game_mode & GM_MULTI) || !xl_trigger_texture_valid(action)) return 1;
	const auto change = xl_compat::texture_change(action->value);
	for (int link = 0; link < action->num_links; ++link) {
		auto &side = Segments[action->seg[link]].sides[action->side[link]];
		side.tmap_num = static_cast<short>(change.base);
		side.tmap_num2 = static_cast<short>(xl_compat::changed_overlay(side.tmap_num2, change.overlay));
#ifdef __ANDROID__
		if (side.wall_num >= 0 && side.wall_num < Num_walls) escort_route_notify_wall_changed(side.wall_num);
#endif
	}
	// Source audio.Update invalidates routes even for an empty or unchanged action
	flush_fcd_cache_for_sound();
	if (!(action->flags & TF_NO_MESSAGE)) HUD_init_message_literal(HM_DEFAULT, "Walls have been changed!");
	return 0;
}

extern "C" int xl_trigger_illusion_action(const trigger *action)
{
	if (!xl_level_extensions_active() || !action || (action->type != TT_ILLUSION_OFF && action->type != TT_ILLUSION_ON)) return 0;
	if (action->num_links < 0 || action->num_links > MAX_WALLS_PER_LINK) return 1;
	const bool off = action->type == TT_ILLUSION_OFF;
	const auto change = [&](int wall) {
		if (wall < 0 || wall >= Num_walls) return;
		if (off) {
			Walls[wall].flags |= WALL_ILLUSION_OFF;
			kill_stuck_objects(wall);
		} else Walls[wall].flags &= ~WALL_ILLUSION_OFF;
#ifdef __ANDROID__
		escort_route_notify_wall_changed(wall);
#endif
	};
	for (int link = 0; link < action->num_links; ++link) {
		const int segment = action->seg[link], side = action->side[link];
		if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6) continue;
		const auto &source = Segments[segment];
		const int wall = source.sides[side].wall_num;
		if (wall >= 0 && wall < Num_walls) {
			change(wall);
			const int child = source.children[side];
			if (child >= 0 && child <= Highest_segment_index) {
				const int connected = find_connect_side(&Segments[segment], &Segments[child]);
				if (connected >= 0 && connected < 6) change(Segments[child].sides[connected].wall_num);
			}
		}
		if (off && xl_trigger_cue_enabled(action, SOUND_WALL_REMOVED)) {
			vms_vector center;
			compute_center_point_on_side(&center, &Segments[segment], side);
			digi_link_sound_to_pos(SOUND_WALL_REMOVED, segment, side, &center, 0, F1_0);
		}
	}
	return 1;
}
