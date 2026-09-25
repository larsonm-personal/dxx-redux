/* Original D1 saved AI fields with no D2 algorithm equivalent */
#ifndef D1_IN_D2_AI_STORAGE_H
#define D1_IN_D2_AI_STORAGE_H

#include "fix.h"

/* Original 30-byte D1 AI object record, distinct from the D2 wire layout */
typedef struct d1_ai_static_rw {
	ubyte behavior;
	sbyte flags[11];
	short hide_segment;
	short hide_index;
	short path_length;
	short cur_path_index;
	short follow_path_start_seg;
	short follow_path_end_seg;
	int danger_laser_signature;
	short danger_laser_num;
} __pack__ d1_ai_static_rw;

/* Keep these in their owning runtime records so object copies, slot reuse
 * and local-AI initialization have the same lifetime as native D1 */
typedef struct d1_ai_static_storage {
	short follow_path_start_seg;
	short follow_path_end_seg;
} d1_ai_static_storage;

typedef struct d1_ai_local_storage {
	fix last_see_time;
	fix last_attack_time;
	fix wait_time;
} d1_ai_local_storage;

#endif
