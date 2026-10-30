#ifndef DXX_XL_LINKED_AUDIO_H
#define DXX_XL_LINKED_AUDIO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct vms_vector;
// Physical samples; translation and forcefield custom-volume scaling belong to callers
// Returns a stable owner identity, or -1 for rejected and one-shot requests
int64_t xl_audio_link_object(int sample, int sound_class, int object, int forever, int volume, int range, int loop_start, int loop_end, int decay, int custom);
int64_t xl_audio_link_position(int sample, int segment, int side, const struct vms_vector *position, int forever, int volume, int range);
// Logical consumers retain the original ID for demo recording
int64_t xl_audio_link_object_recorded(int sample, int sound_class, int object, int forever, int volume, int range, int loop_start, int loop_end, int decay, int custom, int logical);
int64_t xl_audio_link_position_recorded(int sample, int segment, int side, const struct vms_vector *position, int forever, int volume, int range, int logical);
void xl_audio_defer_objects(int deferred);
void xl_audio_sync_objects(void);
// Invalidate spatial paths without replacing owners, channels or playback phase
void xl_audio_update_routes(void);
int xl_audio_destroy_object(int object, int sample);
int xl_audio_destroy_position(int segment, int side, int sample);
int xl_audio_object_count(int object, int sample, int playing_only);
int xl_audio_position_count(int segment, int side, int sample, int playing_only);
#ifdef __cplusplus
}
#endif
#endif
