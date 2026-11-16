#include "bm.h"
#include "cntrlcen.h"
#include "effects.h"
#include "textures.h"
#include "xl_animation_phase.h"
#if defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME)
#include "pog_resources.h"
#endif

static int effect_frame_total(const eclip *ec)
{
#if defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME)
	return xl_pog_effect_frames(ec);
#else
	return ec->vc.num_frames;
#endif
}

static bitmap_index effect_frame_bitmap(const eclip *ec, int frame)
{
#if defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME)
	int i;
	for (i = 0; i < Num_effects; ++i)
		if (ec == &Effects[i]) {
			const int replacement = xl_pog_apply_effect(i, frame);
			if (replacement >= 0) {
				bitmap_index bitmap;
				bitmap.index = replacement;
				return bitmap;
			}
			break;
		}
#endif
	return ec->vc.frames[frame];
}

static int effect_clamp_frame_count(const eclip *ec, int frame_count)
{
	const int frames = effect_frame_total(ec);
	if (frames <= 0)
		return 0;
	if (frame_count < 0)
		return 0;
	if (frame_count >= frames)
		return frame_count % frames;
	return frame_count;
}

void effect_get_loop_state(const eclip *ec, fix64 elapsed_time,
                           int *frame_count, fix *time_left)
{
	fix frame_time;
	fix64 advances, remainder;
	int num_frames;

	frame_time = ec->vc.frame_time;
	num_frames = effect_frame_total(ec);

	if (num_frames <= 0 || frame_time <= 0) {
		*frame_count = 0;
		*time_left = 0;
		return;
	}

	if (elapsed_time <= 0) {
		*frame_count = 0;
		*time_left = frame_time;
		return;
	}

	advances = (elapsed_time - 1) / frame_time;
	remainder = elapsed_time % frame_time;
	*frame_count = (int) (advances % num_frames);
	*time_left = remainder ? (fix) (frame_time - remainder) : 0;
}

void effect_apply_bitmap_state(int effect_num)
{
	eclip *crit_ec, *ec;
	int frame_count;

	ec = &Effects[effect_num];
	if ((ec->changing_wall_texture == -1) &&
	    (ec->changing_object_texture == -1))
		return;

	if (ec->flags & EF_CRITICAL)
		return;

	if (ec->crit_clip != -1 && Control_center_destroyed) {
		crit_ec = &Effects[ec->crit_clip];
		frame_count = effect_clamp_frame_count(crit_ec, crit_ec->frame_count);
		if (ec->changing_wall_texture != -1) {
			Textures[ec->changing_wall_texture] = effect_frame_bitmap(crit_ec, frame_count);
#if defined(ANDROID) && defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME) && defined(OGL_MERGE)
			dxx_xl_wall_mapped_phase(effect_num, Textures[ec->changing_wall_texture].index,
			                         effect_frame_total(crit_ec), crit_ec->vc.frame_time, frame_count, crit_ec->time_left, crit_ec->flags & EF_STOPPED);
#endif
		}
		if (ec->changing_object_texture != -1)
			ObjBitmaps[ec->changing_object_texture] = effect_frame_bitmap(crit_ec, frame_count);
		return;
	}

	frame_count = effect_clamp_frame_count(ec, ec->frame_count);
	if (ec->changing_wall_texture != -1) {
		Textures[ec->changing_wall_texture] = effect_frame_bitmap(ec, frame_count);
#if defined(ANDROID) && defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME) && defined(OGL_MERGE)
		dxx_xl_wall_mapped_phase(effect_num, Textures[ec->changing_wall_texture].index,
		                         effect_frame_total(ec), ec->vc.frame_time, frame_count, ec->time_left, ec->flags & EF_STOPPED);
#endif
	}
	if (ec->changing_object_texture != -1)
		ObjBitmaps[ec->changing_object_texture] = effect_frame_bitmap(ec, frame_count);
}

void reset_special_effects_to_time(fix64 elapsed_time)
{
	int frame_count, i;
	fix time_left;

	for (i = 0; i < Num_effects; i++) {
		Effects[i].segnum = -1;
		Effects[i].sidenum = -1;
		Effects[i].flags &= ~(EF_STOPPED | EF_ONE_SHOT);
		effect_get_loop_state(&Effects[i], elapsed_time, &frame_count, &time_left);
		Effects[i].frame_count = frame_count;
		Effects[i].time_left = time_left;
	}

	for (i = 0; i < Num_effects; i++)
		effect_apply_bitmap_state(i);
}
