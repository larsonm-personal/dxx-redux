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

#include "render_gameplay_view.h"
#include <string.h>
#include "render.h"
#include "endlevel.h"
#include "gr.h"
#include "game.h"
#include "screens.h"
#include "automap.h"

/* The collector traverses the same prepared rows as render_mine */
extern short render_obj_list[MAX_RENDER_SEGS + N_EXTRA_OBJ_LISTS][OBJS_PER_SEG];

int render_collect_view_objects(fix eye_offset, short *objects)
{
	int count = 0, nn, start;
	if (Endlevel_sequence || !Viewer || !grd_curcanv ||
	    grd_curcanv->cv_bitmap.bm_w <= 0 || grd_curcanv->cv_bitmap.bm_h <= 0)
		return -1;
	g3_start_frame_projection();
	start = render_setup_view(eye_offset);
	render_start_frame();
#ifdef DXX_BUILD_DESCENT_II
	build_segment_list(start, 0);
#else
	build_segment_list(start);
#endif
	build_object_lists(N_render_segs);
	for (nn = N_render_segs; nn--;) {
		int row = nn, column = 0;
		const int segnum = Render_list[nn];
		if (segnum == -1 || visited[segnum] == 255)
			continue;
		visited[segnum] = 255;
		while (render_obj_list[row][column] != -1) {
			const int objnum = render_obj_list[row][column];
			if (objnum < 0) {
				row = -objnum;
				column = 0;
				continue;
			}
			++column;
			if (Objects[objnum].type != OBJ_ROBOT && Objects[objnum].type != OBJ_PLAYER)
				continue;
			/* Preserve the renderer's historical overwrite of the upper half */
			if (count >= MAX_RENDERED_OBJECTS)
				count /= 2;
			objects[count++] = objnum;
		}
	}
	return count;
}

int render_update_main_view_objects(void)
{
	short objects[MAX_RENDERED_OBJECTS];
	grs_canvas *saved_canvas;
	int count;
	/* Ordinary drawing retains its last candidate list while the map or
	 * endlevel sequence owns the view */
	if (Automap_active || Endlevel_sequence)
		return 1;
	saved_canvas = grd_curcanv;
	gr_set_current_canvas(&Screen_3d_window);
	count = render_collect_view_objects(0, objects);
	gr_set_current_canvas(saved_canvas);
	if (count < 0)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	update_rendered_data(0, Viewer, Rear_view, 0);
	Window_rendered_data[0].num_objects = count;
	memcpy(Window_rendered_data[0].rendered_objects, objects, count * sizeof(objects[0]));
#else
	Num_rendered_objects = count;
	memcpy(Ordered_rendered_object_list, objects, count * sizeof(objects[0]));
#endif
	return 1;
}
