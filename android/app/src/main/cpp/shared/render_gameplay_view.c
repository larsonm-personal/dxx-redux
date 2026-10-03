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

#ifdef ANDROID
#include "android_render_fov.h"
#include "config.h"
#include "newdemo.h"
#include "morph.h"
#ifdef DXX_BUILD_DESCENT_II
#include "guidebot_extensions.h"
extern ubyte RenderingType;
extern void do_render_object(int objnum, int window_num);
#else
extern void do_render_object(int objnum);
#endif

void android_render_record_visible_object(object *obj)
{
	if (Newdemo_state != ND_STATE_RECORDING || obj->render_type == RT_NONE)
		return;
	if (obj->render_type == RT_MORPH) {
		morph_data *md = find_morph_data(obj);
		if (md)
			newdemo_record_morph_frame(md);
	}
	newdemo_record_render_object(obj);
}

void android_render_collect_fov_visibility(fix eye_offset, int window_num)
{
	int nn, start;
#ifndef NDEBUG
	extern ubyte object_rendered[MAX_OBJECTS];
	memset(object_rendered, 0, sizeof(object_rendered));
#endif
	if (Newdemo_state == ND_STATE_RECORDING && eye_offset >= 0) {
#ifdef DXX_BUILD_DESCENT_II
		if (RenderingType == 0)
#endif
			newdemo_record_start_frame(FrameTime);
#ifdef DXX_BUILD_DESCENT_II
		if (RenderingType != 255)
#endif
			newdemo_record_viewer_object(Viewer);
	}
	g3_start_frame_projection();
	start = render_setup_view(eye_offset);
	render_start_frame();
#ifdef DXX_BUILD_DESCENT_II
	Window_rendered_data[window_num].num_objects = 0;
	build_segment_list(start, window_num);
#else
	(void) window_num;
	Num_rendered_objects = 0;
	build_segment_list(start);
#endif
	build_object_lists(N_render_segs);
	for (nn = N_render_segs; nn--;) {
		const int segnum = Render_list[nn];
		int row = nn, column = 0;
		g3s_codes codes;
		if (segnum == -1 || visited[segnum] == 255)
			continue;
		visited[segnum] = 255;
		codes = rotate_list(8, Segments[segnum].verts);
		if (!codes.uand && (Viewer->type != OBJ_ROBOT
#ifndef DXX_BUILD_DESCENT_II
		                   || GameCfg.ClassicDepth
#endif
		                   )) {
#ifdef DXX_BUILD_DESCENT_II
			if (!Automap_visited[segnum])
				escort_route_notify_automap_changed(segnum);
#endif
			Automap_visited[segnum] = 1;
		}
		/* Preserve the original reverse segment and sorted object order,
		 * including overflow behavior and demo-view exclusions */
		while (render_obj_list[row][column] != -1) {
			const int objnum = render_obj_list[row][column];
			if (objnum < 0) {
				row = -objnum;
				column = 0;
				continue;
			}
			++column;
#ifdef DXX_BUILD_DESCENT_II
			do_render_object(objnum, window_num);
#else
			do_render_object(objnum);
#endif
		}
	}
}

#ifdef INTROSPECT_ON
static int visibility_verify;
static unsigned int visibility_checks, visibility_failures;
static ubyte visibility_saved_automap[MAX_SEGMENTS];
static uint64_t visibility_reference;

static uint64_t visibility_fingerprint(int window_num)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	int i, count;
	const short *objects;
#ifdef DXX_BUILD_DESCENT_II
	count = Window_rendered_data[window_num].num_objects;
	objects = Window_rendered_data[window_num].rendered_objects;
#else
	(void) window_num;
	count = Num_rendered_objects;
	objects = Ordered_rendered_object_list;
#endif
#define VISIBILITY_HASH(value) hash = (hash ^ (uint32_t) (value)) * UINT64_C(1099511628211)
	VISIBILITY_HASH(N_render_segs);
	for (i = 0; i < N_render_segs; ++i) { VISIBILITY_HASH(Render_list[i]); }
	VISIBILITY_HASH(count);
	for (i = 0; i < count; ++i) { VISIBILITY_HASH(objects[i]); }
	for (i = 0; i <= Highest_segment_index; ++i) { VISIBILITY_HASH(Automap_visited[i]); }
#undef VISIBILITY_HASH
	return hash;
}

void android_render_visibility_verify_set(int enabled)
{
	visibility_verify = enabled;
	visibility_checks = visibility_failures = 0;
}

int android_render_visibility_verify_enabled(void) { return visibility_verify && Newdemo_state == ND_STATE_NORMAL; }
unsigned int android_render_visibility_verify_checks(void) { return visibility_checks; }
unsigned int android_render_visibility_verify_failures(void) { return visibility_failures; }

void android_render_visibility_verify_begin(void)
{
	memcpy(visibility_saved_automap, Automap_visited, sizeof(visibility_saved_automap));
}

void android_render_visibility_verify_reference(int window_num)
{
	visibility_reference = visibility_fingerprint(window_num);
	memcpy(Automap_visited, visibility_saved_automap, sizeof(visibility_saved_automap));
}

void android_render_visibility_verify_compare(int window_num)
{
	++visibility_checks;
	if (visibility_reference != visibility_fingerprint(window_num))
		++visibility_failures;
}
#endif
#endif
