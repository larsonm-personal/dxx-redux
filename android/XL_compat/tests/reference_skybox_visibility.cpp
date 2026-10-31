#include "skybox_render.h"
#include <iostream>
#include <vector>

namespace skybox_visibility_reference
{
#define SW_CULLING 1
#define ENTER(a, b)
#define PROF_START
#define PROF_END(value)
#define RETURN return;
constexpr int SIDE_IS_QUAD = 1, SIDE_IS_TRI_13 = 3;
struct CFixVector {
	std::array<int, 3> p{};
	CFixVector operator-(const CFixVector &b) const
	{
		return { { p[0] - b.p[0], p[1] - b.p[1], p[2] - b.p[2] } };
	}
	static int Dot(const CFixVector &a, const CFixVector &b)
	{
		return int((double(a.p[0]) * b.p[0] + double(a.p[1]) * b.p[1] + double(a.p[2]) * b.p[2]) / 65536.0);
	}
};
struct CSide {
	int m_nType;
	std::array<int, 6> m_vertices;
	std::array<CFixVector, 2> m_normals;
};
struct CSegment {
	CSide m_sides[6];
	CFixVector center;
	CFixVector SideCenter(int) const
	{
		return center;
	}
} segment;
std::array<CFixVector, 4> vertices;
struct {
	struct {
		struct {
			struct {
				CFixVector vPos;
			} viewer;
		} mine;
	} renderData;
} gameData;
struct {
	struct {
		bool bHaveSkyBox;
	} render;
} gameStates;
struct Options {
	struct {
		struct {
			bool bSkybox;
		} automap;
	} render;
} options;
Options *gameOpts = &options;
struct {
	bool active;
	bool Active() const
	{
		return active;
	}
} automap;
std::vector<int> calls;
struct {
	void SetDepthWrite(bool value)
	{
		calls.push_back(value ? 1 : -1);
	}
} ogl;
void RenderSkyBoxFaces()
{
	calls.push_back(2);
}
constexpr int WALL_OPEN = 4, WALL_CLOAKED = 7, FADE_LEVELS = 31;
struct CWall {
	int nType, cloakValue, target;
	CWall *opposite;
	int IsTriggerTarget() const
	{
		return target;
	}
	CWall *OppositeWall() const
	{
		return opposite;
	}
	bool IsInvisible();
};
#define SEGMENT(index) (&segment)
#define VERTICES       vertices
#include "xl_reference_skybox_visibility.h"
#undef VERTICES
#undef SEGMENT
#undef RETURN
#undef PROF_END
#undef PROF_START
#undef ENTER
#undef SW_CULLING
} // namespace skybox_visibility_reference

void skybox_visibility_source_controls()
{
	using namespace skybox_visibility_reference;
	unsigned cases = 0, visible = 0, second_normal = 0;
	vertices = { CFixVector{ { -65536, -131072, 0 } }, CFixVector{ { 65536, -131072, 1 } }, CFixVector{ { 65536, 131072, 65536 } }, CFixVector{ { -65536, 131072, -65536 } } };
	segment.center = { { 11, -29, 7 } };
	const std::array<std::array<int, 3>, 7> normals{ { { 65536, 0, 0 }, { -65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 }, { 0, 0, -65536 }, { 37837, -37837, 37837 }, { 1, 0, 0 } } };
	for (int type : { 1, 2, 3 })
		for (const auto &a : normals)
			for (const auto &b : normals)
				for (int x : { -131072, -65537, -1, 0, 1, 65535, 131072 })
					for (int y : { -131072, -1, 0, 1, 131072 })
						for (int z : { -131072, -1, 0, 1, 131072 }) {
							auto &s = segment.m_sides[0];
							s = { type, type == 3 ? std::array<int, 6>{ 3, 0, 1, 1, 2, 3 } : std::array<int, 6>{ 0, 1, 2, 2, 3, 0 }, { CFixVector{ a }, CFixVector{ b } } };
							gameData.renderData.mine.viewer.vPos = { { x, y, z } };
							const auto anchor = type == 1 ? segment.center.p : vertices[type == 3 ? 1 : 0].p;
							const xl_compat::SkyboxRenderSide input{ anchor, { a, b }, type == 1 };
							const bool expected = FaceIsVisible(0, 0) != 0;
							const bool actual = xl_compat::skybox_face_visible(input, { x, y, z });
							if (actual != expected) throw std::runtime_error("Pinned skybox face visibility differs");
							visible += actual;
							second_normal += actual && type != 1 && !xl_compat::skybox_face_visible({ anchor, { a, b }, true }, { x, y, z });
							++cases;
						}
	for (bool have : { false, true })
		for (bool map : { false, true })
			for (bool show : { false, true }) {
				gameStates.render.bHaveSkyBox = have;
				automap.active = map;
				options.render.automap.bSkybox = show;
				calls.clear();
				RenderSkyBox(0);
				const auto expected = xl_compat::skybox_render_enabled(have, map, show) ? std::vector<int>{ 1, 2 } : std::vector<int>{};
				if (calls != expected) throw std::runtime_error("Pinned skybox automap/depth gate differs");
			}
	if (!visible || visible == cases || !second_normal) throw std::runtime_error("Skybox visibility controls do not distinguish facing decisions");
	unsigned wall_cases = 0;
	for (int type = 0; type <= 8; ++type)
		for (int cloak = 0; cloak <= 255; ++cloak)
			for (bool target : { false, true })
				for (bool opposite : { false, true })
					for (bool opposite_target : { false, true }) {
						CWall other{ 0, 0, opposite_target ? 7 : -1, nullptr };
						CWall wall{ type, cloak, target ? 3 : -1, opposite ? &other : nullptr };
						if (wall.IsInvisible() != xl_compat::skybox_wall_invisible(type, cloak, target, opposite, opposite_target))
							throw std::runtime_error("Pinned skybox mesh wall inclusion differs");
						xl_compat::Inventory level;
						level.level_version = 27;
						level.segments.resize(2);
						for (auto &segment : level.segments) {
							segment.children.fill(-1);
							for (auto &side : segment.sides) side.wall = -1;
						}
						level.segments[0].function = 10;
						level.segments[0].properties = xl_compat::segment_light_fog;
						level.segments[0].children[2] = 1;
						level.segments[1].children[4] = 0;
						level.segments[0].sides[2].wall = 0;
						level.segments[1].sides[4].wall = opposite ? 1 : -1;
						level.walls.resize(2);
						level.walls[0].type = type;
						level.walls[0].cloak = cloak;
						level.triggers.resize(1);
						if (target) level.triggers[0].targets.push_back({ 0, 2 });
						if (opposite_target) level.triggers[0].targets.push_back({ 1, 4 });
						if (xl_compat::skybox_side_textured(level, 0, 2) == wall.IsInvisible() ||
						    xl_compat::skybox_colored_only_boundary(level, 0, 2) != wall.IsInvisible())
							throw std::runtime_error("Loaded skybox face selection differs from pinned wall policy");
						level.segments[0].sides[2].wall = -1;
						if (xl_compat::skybox_side_textured(level, 0, 2) || !xl_compat::skybox_colored_only_boundary(level, 0, 2))
							throw std::runtime_error("Open colored skybox boundary was treated as textured");
						level.segments[0].children[2] = -1;
						if (!xl_compat::skybox_side_textured(level, 0, 2) || xl_compat::skybox_colored_only_boundary(level, 0, 2))
							throw std::runtime_error("External textured skybox face was treated as colored-only");
						++wall_cases;
					}
	std::cout << "Pinned skybox visibility: " << cases << " faces, " << second_normal << " second-normal decisions, eight automap/depth gates and " << wall_cases << " wall inclusion cases passed\n";
}
