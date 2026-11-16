#ifndef DXX_XL_AFTERBURNER_ANIMATION_H
#define DXX_XL_AFTERBURNER_ANIMATION_H
#ifdef __cplusplus
extern "C" {
#endif
void xl_afterburner_animation_clear(void);
void xl_afterburner_animation_created(int owner);
int xl_afterburner_lifetime(int owner, int lifetime);
struct object;
int xl_drop_afterburner_blobs(const struct object *emitter, int size, int lifetime);
/* Return whether XL selected the frame; negative source frames remain invisible */
int xl_afterburner_frame(int owner, int clip, int timeleft, int frames, int *frame);
#ifdef __cplusplus
}
#include <nlohmann/json_fwd.hpp>
nlohmann::json xl_afterburner_animation_snapshot();
bool xl_afterburner_animation_restore(const nlohmann::json &saved, bool apply);
#endif
#endif
