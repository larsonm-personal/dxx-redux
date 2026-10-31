#include "skybox_render.h"
#include <iostream>

namespace skybox_corona_reference
{
#define ENTER(a, b)
#define RETVAL(value) return value;
constexpr int SEGMENT_FUNC_SKYBOX = 10, WID_VISIBLE_FLAG = 1;
struct CSegment {
	int m_function, properties;
	bool HasOutdoorsProp() const
	{
		return (properties & 16) != 0;
	}
} segment;
CSegment *SEGMENT(int)
{
	return &segment;
}
struct CSegFace {
	struct {
		bool bVisible;
		int widFlags;
		bool bIsLight;
		int nCamera, nSide, nCorona;
	} m_info;
} faces[3];
struct tSegFaces {
	int nFaces;
	CSegFace *pFace;
} SEGFACES[1]{ { 3, faces } };
struct {
	struct {
		struct {
			bool bEnabled;
		} effects;
		struct {
			bool bUse;
		} coronas;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		struct {
			int nCoronas;
		} lights;
		struct {
			struct {
				int nSegments;
				int segments[2];
			} visibility[1];
		} mine;
	} renderData;
} gameData;
struct {
	int calls = 0;
	bool FaceHasCorona(int, int side, void *, void *)
	{
		++calls;
		return side != 1;
	}
} glareRenderer;
#include "xl_reference_skybox_coronas.h"
#undef ENTER
#undef RETVAL
} // namespace skybox_corona_reference

void skybox_coronas_source_controls()
{
	using namespace skybox_corona_reference;
	unsigned cases = 0;
	for (int function = 0; function < 32; ++function)
		for (int properties = 0; properties < 128; ++properties)
			for (unsigned flags = 0; flags < 32; ++flags) {
				segment = { function, properties };
				options.render.effects.bEnabled = true;
				options.render.coronas.bUse = true;
				gameData.renderData.mine.visibility[0] = { 2, { -1, 0 } };
				gameData.renderData.lights.nCoronas = 0;
				glareRenderer.calls = 0;
				for (int side = 0; side < 3; ++side)
					faces[side].m_info = { bool(flags & 1), flags & 2 ? WID_VISIBLE_FLAG : 0, bool(flags & 4), flags & 8 ? 0 : -1, side, -7 };
				// Also distinguish the global feature gate from segment exclusion
				options.render.effects.bEnabled = !(flags & 16);
				const int actual = SetupCoronaFaces();
				const bool segment_enabled = xl_compat::skybox_corona_segment_enabled(function, properties);
				const bool eligible = !(flags & 16) && segment_enabled && (flags & 7) == 7 && !(flags & 8);
				if (actual != (eligible ? 2 : 0) || glareRenderer.calls != (eligible ? 3 : 0))
					throw std::runtime_error("Pinned skybox/outdoors corona eligibility differs");
				for (int side = 0; side < 3; ++side) {
					const int expected = (flags & 16) || !segment_enabled ? -7 : eligible && side != 1 ? (side == 0 ? 1 : 2)
					                                                                                   : 0;
					if (faces[side].m_info.nCorona != expected) throw std::runtime_error("Pinned corona face assignment differs");
				}
				++cases;
			}
	std::cout << "Pinned corona setup: " << cases << " segment/property/face cases, source light queries and ordered assignments passed\n";
}
