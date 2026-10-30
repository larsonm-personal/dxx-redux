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
void xl_audio_defer_objects(int deferred);
void xl_audio_sync_objects(void);
int xl_audio_destroy_object(int object, int sample);
int xl_audio_destroy_position(int segment, int side, int sample);
#ifdef __cplusplus
}
#endif
#endif
