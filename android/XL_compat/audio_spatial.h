#ifndef DXX_XL_AUDIO_SPATIAL_H
#define DXX_XL_AUDIO_SPATIAL_H
#include <cstdint>
#include "lighting.h"
namespace xl_compat
{
struct ClassicRouteGraph;
int audio_point_distance(const FixedLightPoint &a, const FixedLightPoint &b);
int audio_extended_range(int max_distance, int decay);
std::int16_t audio_ear_angle(const FixedLightPoint &right, const FixedLightPoint &up, const FixedLightPoint &listener, const FixedLightPoint &sound);
int audio_geometry_distance(const ClassicRouteGraph &graph, const std::vector<std::vector<bool>> &visibility, const std::vector<GeometryRouteNode> &routes, int listener, int sound, const FixedLightPoint &listener_position, const FixedLightPoint &sound_position, int max_distance, int decay);
struct AudioSpatialOutput {
	int volume = 0, pan = 0;
};
// Distance and ear angle come from the owned XL geometry/routing consumers
AudioSpatialOutput audio_spatial_output(int distance, int max_volume, int max_distance, int decay, std::int16_t ear_angle, bool reverse_channels, int hires_profile);
} // namespace xl_compat
#endif
