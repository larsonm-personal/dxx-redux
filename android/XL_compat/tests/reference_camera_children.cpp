#include "camera_children.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace camera_children_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN    return;
#define RETVAL(v) return (v);
#define Assert(v) \
	if (!(v)) throw std::runtime_error("Source child assertion");
#define Int3() throw std::runtime_error("Source child type trap")
constexpr int OBJ_ROBOT = 2, OBJ_PLAYER = 4, OBJ_WEAPON = 5, EARTHSHAKER_ID = 40, ROBOT_SMARTMSL_BLOB_ID = 29;
constexpr int MAX_TARGET_OBJS = 30, MAX_SMART_DISTANCE = 150 * 65536, SHORT_RAND_MAX = 32767, FQ_TRANSWALL = 1, PLAYER_FLAGS_CLOAKED = 1;
constexpr bool IsMultiGame = false, IsCoopGame = false, IsTeamGame = false;
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	fix Mag() const;
	static const fix Dist(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	static const fix NormalizedDir(CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Random();
	const CFixVector operator*(fix) const;
	CFixVector operator-(const CFixVector &b) const
	{
		return { { { v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y, v.coord.z - b.v.coord.z } } };
	}
	CFixVector &operator+=(const CFixVector &b)
	{
		v.coord.x += b.v.coord.x;
		v.coord.y += b.v.coord.y;
		v.coord.z += b.v.coord.z;
		return *this;
	}
};
struct tQuadInt {
	int low, high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected child legacy math");
}
fix QuadSqrt(int, int)
{
	throw std::runtime_error("Unexpected child legacy sqrt");
}
struct {
	struct {
		int nMathFormat = 2;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		int nDifficultyLevel = 0;
		void SRand(int)
		{
			throw std::runtime_error("Unexpected child multiplayer");
		}
	} app;
} gameStates;
struct tParentInfo {
	int nType = OBJ_PLAYER, nObject = 0, nSignature = 1;
};
struct CObject;
struct tObjListRef;
struct CObjListLink {
	CObject *prev = nullptr, *next = nullptr;
	tObjListRef *list = nullptr;
};
struct tObjListRef {
	CObject *head = nullptr, *tail = nullptr;
	int nObjects = 0;
};
struct {
	struct {
		struct {
			tObjListRef all;
		} lists;
	} objData;
} gameData;
struct CObject {
	CObjListLink m_links[3];
	CObjListLink &Links(int i)
	{
		return m_links[i];
	}
	void Link(tObjListRef &, int);
	struct {
		int nType = 255, nId = 0, nSegment = 0;
		struct {
			CFixVector vPos;
		} position;
	} info;
	struct {
		struct {
			tParentInfo parent;
			int nHomingTarget = -1;
		} laserInfo;
		struct {
			bool CLOAKED = false;
		} aiInfo;
	} cType;
	bool companion = false, visible = true;
	int Index() const;
	bool IsGuideBot() const
	{
		return companion;
	}
};
std::array<CObject, 128> objects;
int count = 0;
int CObject::Index() const
{
	return int(this - objects.data());
}
CObject *OBJECT(int index)
{
	return index >= 0 && index < count ? &objects[index] : nullptr;
}
#define OBJ_IDX(p) ((p)->Index())
struct CObjectIterator {
	CObject *m_pObj = nullptr;
	int m_i = 0, m_nLink = 0;
	explicit CObjectIterator(CObject *&);
	int Link()
	{
		return 0;
	}
	CObject *Start();
	CObject *Head();
	bool Done();
	CObject *Next();
	CObject *Step();
	int32_t Size();
};
void RebuildObjectLists()
{
	throw std::runtime_error("Unexpected corrupt source object list");
}
struct {
	int flags = 0;
} players[128];
#define PLAYER(i) players[i]
int GetTeam(int)
{
	return 0;
}
struct CWeaponInfo {
	int children = -1;
} definition;
bool definition_present = true;
CWeaponInfo *WEAPONINFO(int)
{
	return definition_present ? &definition : nullptr;
}
#define WI_Strength(i, d) (40 * 65536)
int glass_calls = 0;
void BlastNearbyGlass(CObject *, fix)
{
	++glass_calls;
}
bool ObjectToObjectVisibility(CObject *, CObject *target, int, float)
{
	return target->visible;
}
std::uint32_t seed = 0;
int draws = 0;
int rand()
{
	seed = seed * 1103515245u + 12345u;
	++draws;
	return (seed >> 16) & 32767;
}
struct Created {
	xl_compat::FixedLightPoint direction, position;
	int parent, type, sound, owner;
};
std::vector<Created> created;
xl_compat::FixedLightPoint point(const CFixVector &v)
{
	return { v.v.coord.x, v.v.coord.y, v.v.coord.z };
}
CFixVector vector(const xl_compat::FixedLightPoint &v)
{
	return { { { v[0], v[1], v[2] } } };
}
int CreateNewWeapon(CFixVector *direction, CFixVector *position, int segment, int parent, int type, int sound)
{
	if (count >= int(objects.size())) throw std::runtime_error("Child test object overflow");
	int owner = count++;
	objects[owner] = {};
	objects[owner].info.nType = OBJ_WEAPON;
	objects[owner].info.nId = type;
	objects[owner].info.nSegment = segment;
	objects[owner].info.position.vPos = *position;
	objects[owner].Link(gameData.objData.lists.all, 0);
	created.push_back({ point(*direction), point(*position), parent, type, sound, owner });
	return owner;
}
#include "xl_reference_camera_children.h"
} // namespace camera_children_reference
void camera_children_source_controls()
{
	using namespace camera_children_reference;
	unsigned cases = 0, children = 0;
	for (int parent_type : { OBJ_PLAYER, OBJ_ROBOT })
		for (int child_type : { -1, 0, 19, 47, 54 })
			for (int targets : { 0, 1, 2, 10, 40 })
				for (int mode = 0; mode < 7; ++mode)
					for (unsigned initial : { 0u, 1u, 17u, 991u }) {
						objects = {};
						gameData.objData.lists.all = {};
						count = targets + 2;
						created.clear();
						glass_calls = 0;
						definition.children = child_type;
						objects[0].info.nType = parent_type;
						auto &weapon = objects[1];
						weapon.info.nType = OBJ_WEAPON;
						weapon.info.nId = child_type == 54 ? 40 : 17;
						weapon.info.nSegment = 7;
						weapon.info.position.vPos = vector({ 117, -389, 713 });
						weapon.cType.laserInfo.parent.nType = parent_type;
						std::vector<xl_compat::CameraSmartTarget> candidates;
						for (int i = 0; i < count; ++i) {
							auto &actor = objects[i];
							if (i >= 2) {
								actor.info.nType = mode == 0 ? OBJ_ROBOT : (i % 3 == 0 ? OBJ_PLAYER : i % 3 == 1 ? OBJ_ROBOT
								                                                                                 : 16);
								actor.info.nId = i;
								actor.info.position.vPos = vector({ 117 + (i - 1) * 65536, -389 + (i % 4) * 32768, 713 - (i % 5) * 16384 });
								if (mode == 1) actor.info.position.vPos.v.coord.x = 117 + 150 * 65536 - (i % 3);
								actor.cType.aiInfo.CLOAKED = mode == 2 && (i % 2 == 0);
								actor.companion = mode == 3 && (i % 2 == 0);
								actor.visible = mode != 4 || i % 2;
								if (mode == 5) actor.info.position.vPos = weapon.info.position.vPos;
							}
							players[i].flags = mode == 2 && i % 2 == 0 ? PLAYER_FLAGS_CLOAKED : 0;
							candidates.push_back({ i, actor.info.nType == OBJ_PLAYER, actor.info.nType == OBJ_ROBOT,
							                       actor.info.nType == OBJ_PLAYER ? bool(players[actor.info.nId].flags) : actor.cType.aiInfo.CLOAKED, actor.companion, point(actor.info.position.vPos) });
						}
						std::vector<int> iteration;
						for (int i = 0; i < count; ++i) iteration.push_back(i);
						if (mode == 6) std::reverse(iteration.begin() + 2, iteration.end());
						auto original_candidates = candidates;
						candidates.clear();
						for (int owner : iteration) {
							objects[owner].Link(gameData.objData.lists.all, 0);
							candidates.push_back(original_candidates[owner]);
						}
						seed = initial;
						draws = 0;
						CreateSmartChildren(&weapon, 6);
						const int source_draws = draws;
						const auto source_seed = seed;
						std::vector<int> choices;
						if (child_type > 0) choices = xl_compat::camera_smart_targets(point(weapon.info.position.vPos), 0, parent_type == OBJ_PLAYER, parent_type == OBJ_ROBOT, candidates,
							                                                          [&](int index) { return objects[index].visible; });
						seed = initial;
						draws = 0;
						if (created.size() != std::size_t(child_type > 0 ? 6 : 0) || glass_calls != (child_type == 54 ? 1 : 0)) throw std::runtime_error("Child count or glass ordering differs from source");
						for (unsigned i = 0; i < created.size(); ++i) {
							int target = xl_compat::camera_smart_choice(choices, [] { return camera_children_reference::rand(); });
							auto goal = target < 0 ? xl_compat::FixedLightPoint{} : point(objects[target].info.position.vPos);
							auto direction = xl_compat::camera_smart_direction(point(weapon.info.position.vPos), target < 0 ? nullptr : &goal, [] { return camera_children_reference::rand(); });
							const auto &expected = created[i];
							if (direction != expected.direction || expected.position != point(weapon.info.position.vPos) || expected.parent != 1 ||
							    expected.type != child_type || expected.sound != int(i == 0) || objects[expected.owner].cType.laserInfo.nHomingTarget != target)
								throw std::runtime_error("Camera smart child target/direction differs from source");
							++children;
						}
						if (draws != source_draws || seed != source_seed) throw std::runtime_error("Camera smart child RNG ordering differs from source");
						++cases;
					}
	std::cout << "Camera smart children source: " << cases << " target profiles and " << children << " child directions/identities passed\n";
}
