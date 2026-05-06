/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Special effects, such as rotating fans, electrical walls, and
 * other cool animations.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "gr.h"
#include "inferno.h"
#include "game.h"
#include "vclip.h"
#include "effects.h"
#include "bm.h"
#include "u_mem.h"
#include "textures.h"
#include "cntrlcen.h"
#include "dxxerror.h"

int Num_effects;
eclip Effects[MAX_EFFECTS];

static int effect_clamp_frame_count(const eclip *ec, int frame_count)
{
	if (ec->vc.num_frames <= 0)
		return 0;
	if (frame_count < 0)
		return 0;
	if (frame_count >= ec->vc.num_frames)
		return frame_count % ec->vc.num_frames;
	return frame_count;
}

void effect_get_loop_state(const eclip *ec, fix64 elapsed_time, int *frame_count, fix *time_left)
{
	fix frame_time;
	fix64 advances, remainder;
	int num_frames;

	frame_time = ec->vc.frame_time;
	num_frames = ec->vc.num_frames;

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
	*frame_count = (int)(advances % num_frames);
	*time_left = remainder ? (fix)(frame_time - remainder) : 0;
}

void effect_apply_bitmap_state(int effect_num)
{
	eclip *crit_ec, *ec;
	int frame_count;

	ec = &Effects[effect_num];
	if ((ec->changing_wall_texture == -1) && (ec->changing_object_texture == -1))
		return;

	if (ec->flags & EF_CRITICAL)
		return;

	if (ec->crit_clip != -1 && Control_center_destroyed) {
		crit_ec = &Effects[ec->crit_clip];
		frame_count = effect_clamp_frame_count(crit_ec, crit_ec->frame_count);
		if (ec->changing_wall_texture != -1)
			Textures[ec->changing_wall_texture] = crit_ec->vc.frames[frame_count];
		if (ec->changing_object_texture != -1)
			ObjBitmaps[ec->changing_object_texture] = crit_ec->vc.frames[frame_count];
		return;
	}

	frame_count = effect_clamp_frame_count(ec, ec->frame_count);
	if (ec->changing_wall_texture != -1)
		Textures[ec->changing_wall_texture] = ec->vc.frames[frame_count];
	if (ec->changing_object_texture != -1)
		ObjBitmaps[ec->changing_object_texture] = ec->vc.frames[frame_count];
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

void init_special_effects()
{
	int i;

	for (i=0;i<Num_effects;i++)
		Effects[i].time_left = Effects[i].vc.frame_time;
}

void reset_special_effects()
{
	int i;

	for (i=0;i<Num_effects;i++) {
		Effects[i].segnum = -1;					//clear any active one-shots
		Effects[i].flags &= ~(EF_STOPPED|EF_ONE_SHOT);		//restart any stopped effects

		//reset bitmap, which could have been changed by a crit_clip
		effect_apply_bitmap_state(i);

	}
}

void do_special_effects()
{
	int i;
	eclip *ec;

	for (i=0,ec=Effects;i<Num_effects;i++,ec++) {

		if ((Effects[i].changing_wall_texture == -1) && (Effects[i].changing_object_texture==-1) )
			continue;

		if (ec->flags & EF_STOPPED)
			continue;

		ec->time_left -= FrameTime;

		while (ec->time_left < 0) {

			ec->time_left += ec->vc.frame_time;
			
			ec->frame_count++;
			if (ec->frame_count >= ec->vc.num_frames) {
				if (ec->flags & EF_ONE_SHOT) {
					Assert(ec->segnum!=-1);
					Assert(ec->sidenum>=0 && ec->sidenum<6);
					Assert(ec->dest_bm_num!=0 && Segments[ec->segnum].sides[ec->sidenum].tmap_num2!=0);
					Segments[ec->segnum].sides[ec->sidenum].tmap_num2 = ec->dest_bm_num | (Segments[ec->segnum].sides[ec->sidenum].tmap_num2&0xc000);		//replace with destoyed
					ec->flags &= ~EF_ONE_SHOT;
					ec->segnum = -1;		//done with this
				}

				ec->frame_count = 0;
			}
		}

		if (ec->flags & EF_CRITICAL)
			continue;

		effect_apply_bitmap_state(i);

	}
}

void restore_effect_bitmap_icons()
{
	int i;
	
	for (i=0;i<Num_effects;i++)
		if (! (Effects[i].flags & EF_CRITICAL))	{
			if (Effects[i].changing_wall_texture != -1)
				Textures[Effects[i].changing_wall_texture] = Effects[i].vc.frames[0];
	
			if (Effects[i].changing_object_texture != -1)
				ObjBitmaps[Effects[i].changing_object_texture] = Effects[i].vc.frames[0];
		}
			//if (Effects[i].bm_ptr != -1)
			//	*Effects[i].bm_ptr = &GameBitmaps[Effects[i].vc.frames[0].index];
}

//stop an effect from animating.  Show first frame.
void stop_effect(int effect_num)
{
	eclip *ec = &Effects[effect_num];
	
	//Assert(ec->bm_ptr != -1);

	ec->flags |= EF_STOPPED;

	ec->frame_count = 0;
	//*ec->bm_ptr = &GameBitmaps[ec->vc.frames[0].index];

	if (ec->changing_wall_texture != -1)
		Textures[ec->changing_wall_texture] = ec->vc.frames[0];
	
	if (ec->changing_object_texture != -1)
		ObjBitmaps[ec->changing_object_texture] = ec->vc.frames[0];

}

//restart a stopped effect
void restart_effect(int effect_num)
{
	Effects[effect_num].flags &= ~EF_STOPPED;

	//Assert(Effects[effect_num].bm_ptr != -1);
}

/*
 * reads n eclip structs from a PHYSFS_file
 */
int eclip_read_n(eclip *ec, int n, PHYSFS_file *fp)
{
	int i, j;

	for (i = 0; i < n; i++) {
		ec[i].vc.play_time = PHYSFSX_readFix(fp);
		ec[i].vc.num_frames = PHYSFSX_readInt(fp);
		ec[i].vc.frame_time = PHYSFSX_readFix(fp);
		ec[i].vc.flags = PHYSFSX_readInt(fp);
		ec[i].vc.sound_num = PHYSFSX_readShort(fp);
		for (j = 0; j < VCLIP_MAX_FRAMES; j++)
			ec[i].vc.frames[j].index = PHYSFSX_readShort(fp);
		ec[i].vc.light_value = PHYSFSX_readFix(fp);
		ec[i].time_left = PHYSFSX_readFix(fp);
		ec[i].frame_count = PHYSFSX_readInt(fp);
		ec[i].changing_wall_texture = PHYSFSX_readShort(fp);
		ec[i].changing_object_texture = PHYSFSX_readShort(fp);
		ec[i].flags = PHYSFSX_readInt(fp);
		ec[i].crit_clip = PHYSFSX_readInt(fp);
		ec[i].dest_bm_num = PHYSFSX_readInt(fp);
		ec[i].dest_vclip = PHYSFSX_readInt(fp);
		ec[i].dest_eclip = PHYSFSX_readInt(fp);
		ec[i].dest_size = PHYSFSX_readFix(fp);
		ec[i].sound_num = PHYSFSX_readInt(fp);
		ec[i].segnum = PHYSFSX_readInt(fp);
		ec[i].sidenum = PHYSFSX_readInt(fp);
	}
	return i;
}
