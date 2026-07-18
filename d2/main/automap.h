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
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Prototypes for auto-map stuff.
 *
 */

#ifndef _AUTOMAP_H
#define _AUTOMAP_H

#include "player.h"

extern int Automap_active;

#ifdef INTROSPECT_ON
#include "vecmat.h"
typedef struct automap_view_info {
	vms_vector  view_pos;
	vms_vector  view_target;
	vms_matrix  view_matrix;
	fix         viewDist;
	fix         zoom;
	vms_angvec  tangles;
	int         freeflight;
	int         secret_reveal_unfound;
	int         edge_count;
	int         edges_drawn_last_frame;
	int         secret_edge_count;
	int         secret_visible_edge_count;
	int         secret_too_far_edge_count;
	int         secret_edges_drawn_last_frame;
	int         secret_edges_culled_far_dist_last_frame;
	int         secret_label_candidate_count;
	int         secret_label_projected_count;
	int         objective_overlay_mode;
	int         objective_visible_step_count;
	int         objective_label_candidate_count;
	int         objective_label_projected_count;
	int         objective_connector_candidate_count;
	int         objective_connector_drawn_count;
	int         next_objective_count;
} automap_view_info;
int automap_get_view_info(automap_view_info *out);
#endif

extern char Marker_input[40];
extern void do_automap();
extern void automap_clear_visited();
extern void input_demo_apply_recorded_marker_drop(int player_marker_num, const char *message);
extern ubyte Automap_visited[MAX_SEGMENTS];
void DropBuddyMarker(object *objp);

#define NUM_MARKERS         16
#define MARKER_MESSAGE_LEN  40

extern char MarkerMessage[NUM_MARKERS][MARKER_MESSAGE_LEN];
extern int  MarkerObject[NUM_MARKERS];
extern vms_vector MarkerPoint[NUM_MARKERS];

#endif
