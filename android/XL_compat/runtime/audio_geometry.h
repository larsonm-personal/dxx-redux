#ifndef DXX_XL_RUNTIME_AUDIO_GEOMETRY_H
#define DXX_XL_RUNTIME_AUDIO_GEOMETRY_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "visibility.h"
#include "../audio_spatial.h"
namespace xl_compat
{
// Construct after loading a mine; the owning audio bank discards this on level change
class ClassicAudioSpatial
{
  public:
	ClassicAudioSpatial();
	AudioSpatialOutput output(int listener_segment, int sound_segment, const FixedLightPoint &listener, const FixedLightPoint &sound, const FixedLightPoint &right, const FixedLightPoint &up, int volume, int range, int decay, bool reverse = false, int hires = 1);

  private:
	ClassicRouteGraph graph;
	std::vector<std::vector<bool>> visibility;
	std::vector<GeometryRouteNode> routes;
	int route_listener = -1;
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
