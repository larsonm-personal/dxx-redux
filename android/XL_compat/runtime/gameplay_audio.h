#ifndef DXX_XL_GAMEPLAY_AUDIO_H
#define DXX_XL_GAMEPLAY_AUDIO_H
#include "../audio_logical.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct vms_vector;
struct object;
// Return handled even when an XL event is silent or its sample is unavailable
int xl_gameplay_weapon_fire(const struct object *weapon, int make_sound);
int xl_gameplay_weapon_explosion(int object, int water);
int xl_gameplay_actor_explosion(int object);
int xl_gameplay_sound_sample(int logical);
int xl_gameplay_sound_play(int sound, int sound_class, int volume, int pan, int loops);
void xl_gameplay_loop_start(int sound, int volume, int loop_start, int loop_end);
void xl_gameplay_loop_volume(int volume);
void xl_gameplay_loop_stop(void);
void xl_gameplay_seismic_begin(void);
void xl_gameplay_seismic_end(void);
int64_t xl_gameplay_sound_object(int sound, int sound_class, int object, int forever, int volume, int range, int loop_start, int loop_end, int decay);
int64_t xl_gameplay_sound_position(int sound, int segment, int side, const struct vms_vector *position, int forever, int volume, int range);
// Legacy engine callers use an int token; saved owner identities remain 64-bit
int xl_gameplay_link_object(int sound, int object, int forever, int volume, int range, int loop_start, int loop_end);
int xl_gameplay_link_position(int sound, int segment, int side, const struct vms_vector *position, int forever, int volume, int range);
void xl_gameplay_record_objects(void);
#ifdef __cplusplus
}
#endif
#endif
