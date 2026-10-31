#include "skybox_render.h"
#include <algorithm>
#include <iostream>

namespace skybox_object_reference
{
#define ENTER(a, b)
#define PROF_START
#define PROF_END(value)
#define RETURN return;
constexpr int RENDER_TYPE_OBJECTS = 5;
struct CSegment {
	int m_objects;
} segments[4];
struct CObject {
	struct {
		int nNextInSeg;
	} info;
} objects[12];
struct List {
	std::vector<int16_t> ids;
	int ToS() const
	{
		return int(ids.size());
	}
	int16_t *Buffer()
	{
		return ids.data();
	}
};
struct {
	struct {
		List skybox;
	} segData;
} gameData;
struct {
	struct {
		int nType, nState, nWindow[1];
	} render;
} gameStates;
std::vector<std::array<int, 2>> calls;
void DoRenderObject(int index, int window)
{
	calls.push_back({ index, window });
}
#define SEGMENT(index) (&segments[index])
#define OBJECT(index)  (&objects[index])
#include "xl_reference_skybox_objects.h"
#undef OBJECT
#undef SEGMENT
#undef RETURN
#undef PROF_END
#undef PROF_START
#undef ENTER
} // namespace skybox_object_reference

void skybox_objects_source_controls()
{
	using namespace skybox_object_reference;
	unsigned cases = 0, drawn = 0;
	std::array<int, 4> order{ 0, 1, 2, 3 };
	do {
		for (unsigned mask = 0; mask < 16; ++mask)
			for (unsigned empty = 0; empty < 16; ++empty)
				for (int window : { 0, 1, 7 }) {
					std::vector<int> skybox;
					gameData.segData.skybox.ids.clear();
					for (unsigned segment = 0; segment < 4; ++segment) {
						const int base = order[segment] * 3;
						segments[segment].m_objects = empty & (1u << segment) ? -1 : base + 1;
						objects[base + 1].info.nNextInSeg = base;
						objects[base].info.nNextInSeg = base + 2;
						objects[base + 2].info.nNextInSeg = -1;
						if (mask & (1u << segment)) {
							skybox.push_back(int(segment));
							gameData.segData.skybox.ids.push_back(int16_t(segment));
						}
					}
					gameStates.render = { -1, -1, { window } };
					calls.clear();
					RenderSkyBoxObjects();
					const auto expected = calls;
					calls.clear();
					xl_compat::visit_skybox_objects(skybox, 12, [](int segment) { return segments[segment].m_objects; }, [](int object) { return objects[object].info.nNextInSeg; }, [window](int object) { DoRenderObject(object, window); });
					if (calls != expected || gameStates.render.nType != RENDER_TYPE_OBJECTS || gameStates.render.nState != 1)
						throw std::runtime_error("Pinned skybox object traversal differs");
					drawn += unsigned(calls.size());
					++cases;
				}
	} while (std::next_permutation(order.begin(), order.end()));
	for (int invalid : { -2, 1, 0 }) {
		bool rejected = false;
		try {
			xl_compat::visit_skybox_objects({ 0 }, 1, [invalid](int) { return invalid; }, [](int) { return 0; }, [](int) {});
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Skybox traversal did not bound an invalid or cyclic list");
	}
	std::cout << "Pinned skybox objects: " << cases << " lists, " << drawn << " ordered object/window calls, empty lists and invalid-chain bounds passed\n";
}
