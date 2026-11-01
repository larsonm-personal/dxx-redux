// Runtime policy audit kept separate from forced-geometry query fixtures
namespace collision_profile_reference
{
static constexpr bool IsMultiGame = false, IsCoopGame = false;
static constexpr int COLLISION_MODEL_BOXES = 2, RT_POLYOBJ = 1;
static constexpr int OBJ_WEAPON = 5, OBJ_MONSTERBALL = 17, OBJ_HOSTAGE = 3, OBJ_POWERUP = 7;
static constexpr int GAUSS_ID = 32, VULCAN_ID = 11;
static struct {
	char nHitboxes = 2;
} extraGameInfo[2];
static struct {
	int m_nCollisionModel = 0;
} missionConfig;
static struct {
	struct {
		bool bNostalgia = false;
		bool bHaveExtraGameInfo[2]{};
	} app;
} gameStates;
struct CObject {
	struct {
		int renderType = RT_POLYOBJ, nType = 16, nId = 0;
	} info;
	struct {
		struct {
			int nModel = 58;
		} polyObjInfo;
	} rType;
	bool mine = false;
	bool IsMine() const
	{
		return mine;
	}
};
#pragma push_macro("EGI_FLAG")
#undef EGI_FLAG
#define EGI_FLAG(flag, on, off, fallback) EGIFlag(extraGameInfo[0].flag, extraGameInfo[1].flag, fallback, on, off)
#include "xl_reference_collision_profile.h"
#pragma pop_macro("EGI_FLAG")
static void compare()
{
	unsigned cases = 0;
	for (int setting : { 0, 1, 2 })
		for (int mission : { 0, 1 })
			for (bool nostalgia : { false, true })
				for (int type : { 2, 3, 4, 5, 7, 16, 17 })
					for (int id : { 0, VULCAN_ID, GAUSS_ID })
						for (int render : { 0, RT_POLYOBJ })
							for (int model : { -1, 58 })
								for (bool mine : { false, true }) {
									extraGameInfo[0].nHitboxes = static_cast<char>(setting);
									missionConfig.m_nCollisionModel = mission;
									gameStates.app.bNostalgia = nostalgia;
									CObject actor;
									actor.info = { render, type, id };
									actor.rType.polyObjInfo.nModel = model;
									actor.mine = mine;
									const bool polygon = !nostalgia && !setting && render == 1 && model >= 0 && !(type == 5 && (id == 11 || id == 32));
									const bool sphere = nostalgia || type == 17 || type == 3 || type == 7 || mine;
									if (bool(UseHitbox(&actor)) != polygon || bool(UseSphere(&actor)) != sphere || CollisionModel() != (mission ? 2 : setting))
										throw std::runtime_error("Extracted source collision policy changed");
									for (int requested : { 0, 1, 2 })
										if (CollisionModel(requested) != requested) throw std::runtime_error("Explicit source collision model changed");
									++cases;
								}
	extraGameInfo[0].nHitboxes = 2;
	missionConfig.m_nCollisionModel = 0;
	gameStates.app.bNostalgia = false;
	CObject camera;
	if (UseHitbox(&camera) || UseSphere(&camera) || CollisionModel() != 2)
		throw std::runtime_error("Default source camera profile differs from playerprofile nHitboxes=2");
	std::cout << "Source collision selection: " << cases << " single-player profiles passed; default setting2 selects no polygon hitbox, model2\n";
	std::cout << "Forced hitbox geometry fixtures do not establish default runtime camera selection\n";
}
} // namespace collision_profile_reference
