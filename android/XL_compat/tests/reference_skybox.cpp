// Execute the actual collision body with independently recorded external queries
#include "skybox_collision.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace skybox_reference
{
#define ENTER(a, b)
#define RETVAL(value) return value;
#define I2X(value)    ((value) * 65536)
constexpr int OBJ_WEAPON = 5, SEGMENT_FUNC_SKYBOX = 10, OF_SHOULD_BE_DEAD = 2, HIT_NONE = 0;
using Point = std::array<int, 3>;
using Call = std::array<int, 8>;
std::vector<Call> calls;
bool transparent_result;
int locate_result;
struct CSegment {
	int m_function, index;
	std::array<int, 6> m_children;
	bool CheckForTranspPixel(const Point &point, int side, int face)
	{
		calls.push_back({ 1, index, point[0], point[1], point[2], side, face, 0 });
		return transparent_result;
	}
};
std::vector<CSegment> segments;
CSegment *segment(int index)
{
	return &segments.at(std::size_t(index));
}
#define SEGMENT(index) segment(index)
struct {
	struct {
		bool bHaveSkyBox;
	} render;
} gameStates;
struct {
	struct {
		std::vector<int> skybox;
	} segData;
} gameData;
int FindSegByPos(const Point &point, int start, int exhaustive, int skybox)
{
	calls.push_back({ 2, start, point[0], point[1], point[2], exhaustive, skybox, 0 });
	return locate_result;
}
struct CPhysSimData {
	struct {
		int nSegment, nSide, nSideSegment, nFace, nType;
		Point vPoint;
	} hitResult;
	Point vNewPos;
};
struct CObject {
	struct {
		int nType, xLifeLeft, nFlags;
	} info;
	int32_t HandleWallCollision(CPhysSimData &);
	void UpdateLife(int value)
	{
		calls.push_back({ 3, value, 0, 0, 0, 0, 0, 0 });
		info.xLifeLeft = value;
	}
};
#include "xl_reference_skybox_collision.h"
#undef SEGMENT
#undef I2X
#undef RETVAL
#undef ENTER
} // namespace skybox_reference

void skybox_collision_source_controls()
{
	using namespace skybox_reference;
	unsigned cases = 0, exits = 0, entries = 0;
	for (unsigned mask = 0; mask < 8; ++mask)
		for (int hit_segment : { -1, 0, 1, 2 })
			for (int side = 0; side < 6; ++side)
				for (int child : { -2, -1, 0, 2 })
					for (int type : { 0, 2, 4, 5, 9, 20 })
						for (int life : { -65536, 0, 65535, 65536, 65537, INT32_MAX })
							for (bool transparent : { false, true })
								for (int found : { -1, 0, 1, 2 }) {
									xl_compat::Inventory inventory;
									inventory.segments.resize(3);
									segments.clear();
									gameData.segData.skybox.clear();
									for (int i = 0; i < 3; ++i) {
										auto &s = inventory.segments[std::size_t(i)];
										s.function = mask & (1u << i) ? 10 : 0;
										s.children.fill(child);
										segments.push_back({ s.function, i, s.children });
										if (s.function == 10) gameData.segData.skybox.push_back(i);
									}
									gameStates.render.bHaveSkyBox = mask != 0;
									transparent_result = transparent;
									locate_result = found;
									const int face = side % 2, side_segment = (hit_segment + 2) % 3;
									const Point point{ -17, 21, 901 }, destination{ 313, -999, 2 };
									CObject object{ { type, life, 0x41 } };
									CPhysSimData state{ { hit_segment, side, side_segment, face, 1, point }, destination };
									calls.clear();
									if (object.HandleWallCollision(state) != 1) throw std::runtime_error("Unexpected source skybox collision return");
									const auto expected_calls = calls;
									calls.clear();
									const xl_compat::SkyboxCollisionWorld world(inventory);
									if (world.segments() != gameData.segData.skybox) throw std::runtime_error("Skybox collision list ordering differs");
									const auto actual = world.collide(type == OBJ_WEAPON, life, { hit_segment, side, side_segment, face, point, destination }, [&](int index, const Point &p, int s, int f) { return segment(index)->CheckForTranspPixel(p, s, f); }, [&](const Point &p, int start, bool exhaustive, bool skybox) { return FindSegByPos(p, start, exhaustive, skybox); });
									if (actual.retire) calls.push_back({ 3, 0, 0, 0, 0, 0, 0, 0 });
									if (state.hitResult.nSegment != actual.segment || (state.hitResult.nType == HIT_NONE) != actual.pass ||
									    object.info.xLifeLeft != (actual.retire ? 0 : life) || object.info.nFlags != (0x41 | (actual.retire ? OF_SHOULD_BE_DEAD : 0)) ||
									    calls != expected_calls) throw std::runtime_error("Pinned skybox collision or external query sequence differs");
									++cases;
									exits += actual.retire;
									entries += actual.segment != hit_segment;
								}
	if (!exits || !entries) throw std::runtime_error("Skybox source controls did not exercise entry and retirement");
	std::cout << "Pinned skybox wall decisions: " << cases << " cases, " << entries << " entries, " << exits << " retirements; pixel/lookup/lifetime calls and one-second boundary passed\n";
}
