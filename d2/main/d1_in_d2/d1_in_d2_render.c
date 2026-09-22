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

/* New file, largely derived from the original Parallax/Interplay Descent code */

#include <stdlib.h>
#include "d1_in_d2.h"
#include "d1_in_d2_render.h"
#include "config.h"
#include "gameseg.h"
#include "wall.h"
#include "dxxerror.h"

static void add_obj_to_seglist(short render_obj_list[][OBJS_PER_SEG], int objnum,int listnum)
{
	int i,checkn,marker;

	checkn = listnum;

	//first, find a slot

	do {

		for (i=0;render_obj_list[checkn][i] >= 0;i++);

		Assert(i < OBJS_PER_SEG);

		marker = render_obj_list[checkn][i];

		if (marker != -1) {
			checkn = -marker;
			//Assert(checkn < MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS);
			if (checkn >= MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS) {
				Int3();
				return;
			}
		}

	} while (marker != -1);

	//now we have found a slot.  put object in it

	if (i != OBJS_PER_SEG-1) {

		render_obj_list[checkn][i] = objnum;
		render_obj_list[checkn][i+1] = -1;
	}
	else {				//chain to additional list
		int lookn;

		//find an available sublist

		for (lookn=MAX_RENDER_SEGS;lookn<MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS && render_obj_list[lookn][0]!=-1;lookn++);

		//Assert(lookn<MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS);
		if (lookn >= MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS) {
			Int3();
			return;
		}

		render_obj_list[checkn][i] = -lookn;
		render_obj_list[lookn][0] = objnum;
		render_obj_list[lookn][1] = -1;

	}

}

#define SORT_LIST_SIZE 50

typedef struct sort_item {
	int objnum;
	fix dist;
} sort_item;

static sort_item sort_list[SORT_LIST_SIZE];
static int n_sort_items;

//compare function for object sort.
static int sort_func(sort_item *a,sort_item *b)
{
	fix delta_dist;
	object *obj_a,*obj_b;

	delta_dist = a->dist - b->dist;

	if (!GameCfg.ClassicDepth)
		return delta_dist;

	obj_a = &Objects[a->objnum];
	obj_b = &Objects[b->objnum];

	if (abs(delta_dist) < (obj_a->size + obj_b->size)) {		//same position

		//these two objects are in the same position.  see if one is a fireball
		//or laser or something that should plot on top

		if (obj_a->type == OBJ_WEAPON || obj_a->type == OBJ_FIREBALL)
			if (!(obj_b->type == OBJ_WEAPON || obj_b->type == OBJ_FIREBALL))
				return -1;	//a is weapon, b is not, so say a is closer
			else;				//both are weapons
		else
			if (obj_b->type == OBJ_WEAPON || obj_b->type == OBJ_FIREBALL)
				return 1;	//b is weapon, a is not, so say a is farther

		//no special case, fall through to normal return
	}

	return delta_dist;	//return distance
}

int d1_in_d2_build_object_lists(int n_segs, const short *Render_list, short render_obj_list[][OBJS_PER_SEG], const vms_vector *eye)
{
	int nn;

	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	for (nn=0;nn<n_segs;nn++)
		render_obj_list[nn][0] = -1;
	for (nn=MAX_RENDER_SEGS;nn<MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS;nn++)
		render_obj_list[nn][0] = -1;

	for (nn=0;nn<n_segs;nn++) {
		int segnum;

		segnum = Render_list[nn];

		if (segnum != -1) {
			int objnum;
			object *obj;

			for (objnum=Segments[segnum].objects;objnum!=-1;objnum = obj->next) {
				int new_segnum,did_migrate,list_pos;

				obj = &Objects[objnum];

				if (obj->type == OBJ_NONE)
					continue;

				Assert( obj->segnum == segnum );

				if (obj->flags & OF_ATTACHED)
					continue;		//ignore this object

				new_segnum = segnum;
				list_pos = nn;

				if (obj->type != OBJ_CNTRLCEN)		//don't migrate controlcen
				do {
					segmasks m;

					did_migrate = 0;

					m = get_seg_masks(&obj->pos, new_segnum, obj->size, __FILE__, __LINE__);

					if (m.sidemask) {
						int sn,sf;

						for (sn=0,sf=1;sn<6;sn++,sf<<=1)
							if (m.sidemask & sf) {
								segment *seg = &Segments[obj->segnum];

								if (WALL_IS_DOORWAY(seg,sn) & WID_FLY_FLAG) {		//can explosion migrate through
									int child = seg->children[sn];
									int checknp;

									for (checknp=list_pos;checknp--;)
										if (Render_list[checknp] == child) {
											new_segnum = child;
											list_pos = checknp;
											did_migrate = 1;
										}
								}
							}
					}

				} while (did_migrate);

				add_obj_to_seglist(render_obj_list, objnum,list_pos);

			}

		}
	}

	//now that there's a list for each segment, sort the items in those lists
	for (nn=0;nn<n_segs;nn++) {
		int segnum;

		segnum = Render_list[nn];

		if (segnum != -1) {
			int t,lookn,i,n;

			//first count the number of objects & copy into sort list

			lookn = nn;
			i = n_sort_items = 0;
			while ((t=render_obj_list[lookn][i++])!=-1)
				if (t<0)
					{lookn = -t; i=0;}
				else
					if (n_sort_items < SORT_LIST_SIZE-1) {		//add if room
						sort_list[n_sort_items].objnum = t;
						//NOTE: maybe use depth, not dist - quicker computation
						sort_list[n_sort_items].dist = vm_vec_dist(&Objects[t].pos,eye);
						n_sort_items++;
					}


			//now call qsort

			qsort(sort_list,n_sort_items,sizeof(*sort_list),(int (*)(const void *, const void *))sort_func);

			//now copy back into list

			lookn = nn;
			i = 0;
			n = n_sort_items;
			while ((t=render_obj_list[lookn][i])!=-1 && n>0)
				if (t<0)
					{lookn = -t; i=0;}
				else
					render_obj_list[lookn][i++] = sort_list[--n].objnum;
			render_obj_list[lookn][i] = -1;	//mark (possibly new) end
		}
	}
	return 1;
}

