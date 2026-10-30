// Independent adapters for actual source functions and exact call-site guards
#include "../robot_attack_mode.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace disarm_reference
{
constexpr int OBJ_ROBOT = 2, OBJ_PLAYER = 4, OBJ_CNTRLCEN = 9, OBJ_GHOST = 12;
constexpr int ROBOT_IS_HOSTILE = -1, ROBOT_IS_NEUTRAL = 0, ROBOT_IS_FRIENDLY = 1, AIS_RECOVER = 6;
using fix = int;
constexpr int I2X(int n)
{
	return n * 65536;
}
#define ENTER(a, b)
#define RETURN return;
bool robots_hit = false;
#define EGI_FLAG(a, b, c, d) robots_hit
struct {
	struct {
		struct {
			int bRobotsKillRobots = 0, bRobotsFiring = 1;
		} cheats;
	} app;
} gameStates;
struct CFixVector {
	int x = 0;
	static int Dist(const CFixVector &a, const CFixVector &b)
	{
		return a.x > b.x ? a.x - b.x : b.x - a.x;
	}
};
int hits = 0, drains = 0, next_fires = 0, shots = 0, decisions = 0;
struct CObject {
	int index = 0, m_nAttackRobots = -1;
	bool is_static = false, cloaked = false;
	struct {
		int nType = OBJ_ROBOT, nId = 0, xSize = 65536;
		struct {
			CFixVector vPos;
		} position;
	} info;
	struct {
		struct {
			int GOAL_STATE = 0;
		} aiInfo;
	} cType;
	bool IsRobot()
	{
		return info.nType == OBJ_ROBOT;
	}
	bool IsReactor()
	{
		return info.nType == OBJ_CNTRLCEN;
	}
	bool IsPlayer()
	{
		return info.nType == OBJ_PLAYER;
	}
	bool IsStatic()
	{
		return is_static;
	}
	bool Cloaked()
	{
		return cloaked;
	}
	void CollidePlayerAndNastyRobot(CObject *, const CFixVector &)
	{
		++hits;
	}
	void DrainEnergy()
	{
		++drains;
	}
	bool AttacksRobots();
	bool AttacksPlayer();
	bool AttacksObject(CObject *);
#include "xl_reference_disarm_methods.h"
};
std::array<CObject, 5> objects;
CObject player;
#define OBJECT(n)   gameData.Object(n)
#define OBJ_IDX(p)  ((p)->index)
#define OBJPOS(p)   (&(p)->info.position)
#define TARGETOBJ   (&player)
#define LOCALOBJECT (&player)
struct tAILocalInfo {
	int pNextrimaryFire = 0;
};
struct tRobotInfo {
	int attackType = 1;
};
tRobotInfo robot_info;
#define ROBOTINFO(n) ((n) == 0 ? &robot_info : nullptr)
struct CGameData {
	struct {
		tAILocalInfo localInfo[5];
	} aiData;
	struct {
		int nLastObject[2]{ 4, 4 };
		CObject *objects = disarm_reference::objects.data();
	} objData;
	CObject *Object(int32_t);
};
CGameData gameData;
#include "xl_reference_disarm_lookup.h"
void SetNextFireTime(CObject *, tAILocalInfo *, tRobotInfo *, int)
{
	++next_fires;
}
struct CTrigger {
	int m_nLinks = 0;
	int m_segments[10]{}, m_sides[10]{};
	int32_t DoDisarmRobots();
};
#include "xl_reference_disarm.h"
#include "xl_reference_disarm_guards.h"
} // namespace disarm_reference
void disarm_source_controls()
{
	using namespace disarm_reference;
	const auto require = [](bool v, const char *e) {if(!v)throw std::runtime_error(e); };
	unsigned cases = 0;
	for (int mode : { -2, -1, 0, 1, 2 })
		for (int type : { OBJ_ROBOT, OBJ_PLAYER, OBJ_CNTRLCEN, OBJ_GHOST })
			for (int firing : { 0, 1 }) {
				auto &robot = objects[0];
				robot = CObject{};
				robot.info.nType = type;
				robot.m_nAttackRobots = mode;
				gameStates.app.cheats.bRobotsFiring = firing;
				const bool allowed = xl_compat::robot_attacks_player(mode, type == OBJ_ROBOT || type == OBJ_CNTRLCEN, firing);
				require(robot.Disarmed() == xl_compat::robot_disarmed(mode) && robot.AttacksPlayer() == allowed, "Attack-mode predicate differs from XL");
				player = CObject{};
				player.info.nType = OBJ_PLAYER;
				hits = drains = next_fires = shots = decisions = 0;
				DoAIRobotHitAttack(&robot, &player, &player.info.position.vPos);
				fire_guard(&robot);
				thief_guard(&robot);
				require(hits == int(allowed) && drains == hits && next_fires == hits && shots == hits && decisions == int(mode < 0), "Source disarm consumers disagree in firing, melee or thief decisions");
				++cases;
			}
	for (int last : { 0, 3, 4 })
		for (int mode : { -1, 0, 1 })
			for (int type : { OBJ_ROBOT, OBJ_PLAYER, OBJ_CNTRLCEN, OBJ_GHOST })
				for (int side : { -32768, -1, 0, 5, 42 })
					for (int index : { -32768, -1, 0, 4, 5, 32767 }) {
						gameData.objData.nLastObject[0] = last;
						for (int i = 0; i < int(objects.size()); ++i) {
							objects[i] = CObject{};
							objects[i].index = i;
							objects[i].info.nType = type;
							objects[i].m_nAttackRobots = mode;
						}
						CTrigger action;
						action.m_nLinks = 3;
						action.m_segments[0] = action.m_segments[1] = index;
						action.m_sides[0] = action.m_sides[1] = side;
						action.m_segments[2] = 32767;
						action.m_sides[2] = 42;
						require(action.DoDisarmRobots() == 1, "Source disarm return changed");
						for (int i = 0; i < int(objects.size()); ++i) require(objects[i].m_nAttackRobots == (i == index && index <= last && side < 0 && type == OBJ_ROBOT ? 0 : mode), "Source disarm target selection changed");
						++cases;
					}
	std::cout << "PASS: " << cases << " pinned disarm target, attack-mode, melee and ranged/thief guard controls\n";
}
