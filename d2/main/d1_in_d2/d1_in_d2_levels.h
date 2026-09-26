#ifndef D1_IN_D2_LEVELS_H
#define D1_IN_D2_LEVELS_H

#include "object.h"
#include "switch.h"

/* Both inputs are serialized D1 texture references, with packed orientation
 * on the overlay. Resolve against the published bank and validate before
 * changing either value. new_file_format distinguishes RDL from SDL layout
 * Native assets retain source IDs; the legacy editor path uses D2 slots */
int d1_in_d2_decode_level_textures(short *primary, short *overlay, int new_file_format);

/* Explicit legacy D1-source -> D2-slot conversion, independent of active
 * native assets. Asset-table adapters supply their source layout directly */
short d1_in_d2_legacy_texture(short source, int d1_pig_present, int new_file_format);

/* Bind source object references after common model-name remapping */
void d1_in_d2_fixup_level_object(object *obj, int level_version);

/* D1 has its original sound sources, without D2's random water/lava ambience
 * Returns 1 after preparing native flags, 0 for the engine's D2 preparation */
int d1_in_d2_initialize_level_ambience(void);

/* Decode old flag-based records without narrowing the source link count first
 * native_d1 selects D1 semantics explicitly; old D2 formats retain their adapter
 * Link segment bounds are checked against the loaded/restored world by its loader */
int d1_in_d2_decode_trigger(trigger *out, const v29_trigger *source, int native_d1);
/* Level files may contain stale editor flags and links; save translation stays strict */
int d1_in_d2_decode_level_trigger(trigger *out, const v29_trigger *source);
/* Returns 1 and the original flags for a native record, 0 for an ordinary D2 record */
int d1_in_d2_trigger_source_flags(const trigger *source, short *flags);
int d1_in_d2_trigger_source_link(const trigger *source);
void d1_in_d2_write_trigger_storage(rewind_file *fp);
int d1_in_d2_read_trigger_storage(rewind_file *fp, int swap, int apply);
/* Bind native wall backlinks after world loading: -1 D2, 0 invalid, 1 bound */
int d1_in_d2_bind_trigger_links(int trigger_num);

/* Activation/crossing return -1 for D2, 0 to send a trigger hit, 1 to stop
 * Activation runs after common trigger/player bounds, connection/travel guards
 * Crossing also owns native actor eligibility and paired-wall completion */
int d1_in_d2_activate_trigger(int trigger_num, int player_num);
int d1_in_d2_cross_trigger(int trigger_num, segment *seg, int side, int objnum, int shot);

/* Complete native campaign operations; 0 leaves ordinary D2 handling untouched */
int d1_in_d2_finish_level(int secret);
/* After shared death/life accounting, before game-specific respawn or travel */
int d1_in_d2_finish_death(void);
/* Native bonus arithmetic; common score display/hostage bonuses stay shared */
int d1_in_d2_level_bonuses(int level_points, int *skill, int *shields, int *energy);

#endif
