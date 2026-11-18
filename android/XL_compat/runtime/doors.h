#ifndef DXX_XL_DOORS_H
#define DXX_XL_DOORS_H
#ifdef __cplusplus
extern "C" {
#endif
struct active_door;
int xl_door_open(int segment, int side);
int xl_door_close(int segment, int side, int force);
int xl_door_frame_process(void);
int xl_door_state_valid(const struct active_door *doors, int count);
void xl_door_phases_clear(void);
int xl_door_record_frame(int segment, int side, int clip, int frame);
struct _grs_bitmap;
struct _grs_bitmap *xl_door_side_bitmap(int segment, int side, int texture, struct _grs_bitmap *ordinary);
#ifdef __cplusplus
}
#include <nlohmann/json_fwd.hpp>
namespace xl_compat
{
struct Inventory;
struct ScopedTexturePlayback;
} // namespace xl_compat
namespace xl_assets
{
struct ObjectDependencies;
}
void xl_door_phases_bind(const xl_compat::Inventory &level, const xl_assets::ObjectDependencies &tables,
                         const xl_compat::ScopedTexturePlayback *playback);
nlohmann::json xl_door_phases_snapshot();
bool xl_door_phases_restore(const nlohmann::json &state, bool apply);
#endif
#endif
