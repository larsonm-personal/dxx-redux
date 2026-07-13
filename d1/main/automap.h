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
 * Prototypes for auto-map stuff.
 *
 */



#ifndef _AUTOMAP_H
#define _AUTOMAP_H

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
	int         secret_edge_count;
	int         secret_visible_edge_count;
	int         secret_too_far_edge_count;
	int         secret_edges_drawn_last_frame;
	int         secret_edges_culled_far_dist_last_frame;
	int         secret_label_candidate_count;
	int         secret_label_projected_count;
	int         objective_overlay_enabled;
	int         objective_label_candidate_count;
	int         objective_label_projected_count;
	int         objective_connector_candidate_count;
	int         objective_connector_drawn_count;
} automap_view_info;
int automap_get_view_info(automap_view_info *out);
#endif

extern void do_automap();
extern void automap_clear_visited();
extern ubyte Automap_visited[MAX_SEGMENTS];

#endif
