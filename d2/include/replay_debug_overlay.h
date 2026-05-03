/* replay_debug_overlay.h -- robot/object number labels during input demo replay
 *
 * During 3D rendering, do_render_object() accumulates screen positions and
 * object numbers into g_replay_robot_labels[].  After the 3D pass, the labels
 * are drawn as text at the projected screen position of each object.
 * Also draws a frame counter in the upper-left corner.
 *
 * Active whenever input_demo_replay_is_loaded() is true (replay mode only).
 * Works on all platforms (no ANDROID guard). */

#ifndef REPLAY_DEBUG_OVERLAY_H
#define REPLAY_DEBUG_OVERLAY_H

#define REPLAY_ROBOT_LABEL_MAX 128

struct replay_robot_label {
	int sx, sy;    /* projected screen position */
	int objnum;
	int objtype;
};

extern struct replay_robot_label g_replay_robot_labels[REPLAY_ROBOT_LABEL_MAX];
extern int g_replay_robot_label_count;

#endif /* REPLAY_DEBUG_OVERLAY_H */
