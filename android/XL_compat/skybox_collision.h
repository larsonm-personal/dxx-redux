#ifndef DXX_XL_SKYBOX_COLLISION_H
#define DXX_XL_SKYBOX_COLLISION_H
#include "inventory.h"
#include <array>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct SkyboxWallContact {
	int segment, side, side_segment, face;
	std::array<int, 3> point, destination;
};
struct SkyboxWallDecision {
	int segment;
	bool pass = false, retire = false;
};

// Source collision decisions only; lookup and texture-pixel consumers stay explicit
class SkyboxCollisionWorld
{
	struct Segment {
		bool skybox;
		std::array<int, 6> children;
	};
	std::vector<Segment> geometry;
	std::vector<int> skyboxes;

  public:
	explicit SkyboxCollisionWorld(const Inventory &level)
	{
		for (const auto &segment : level.segments) {
			if (segment.function == 10) skyboxes.push_back(int(geometry.size()));
			geometry.push_back({ segment.function == 10, segment.children });
		}
	}
	const std::vector<int> &segments() const
	{
		return skyboxes;
	}
	template <class Transparent, class Locate>
	SkyboxWallDecision collide(bool weapon, int life, const SkyboxWallContact &hit, Transparent transparent, Locate locate) const
	{
		SkyboxWallDecision result{ hit.segment };
		if (skyboxes.empty() || !weapon || hit.segment < 0) return result;
		const auto &segment = geometry.at(std::size_t(hit.segment));
		if (segment.skybox) {
			result.retire = segment.children.at(std::size_t(hit.side)) < 0 && life > 65536;
			result.pass = true;
		} else if (transparent(hit.side_segment, hit.point, hit.side, hit.face)) {
			const int found = locate(hit.destination, skyboxes.front(), true, true);
			if (found >= 0 && geometry.at(std::size_t(found)).skybox) {
				result.segment = found;
				result.pass = true;
			}
		}
		return result;
	}
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
