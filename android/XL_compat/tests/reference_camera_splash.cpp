#include "camera_splash.h"
#include "camera_flash.h"
#include "robot_damage.h"
#include "static_contact.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
namespace camera_splash_reference
{
using fix = std::int32_t;
using fixang = std::int16_t;
#define ENTER(a, b)
#define RETVAL(v)   return (v);
#define RETURN      return;
#define IsMultiGame false
#define OBJPOS(p)   (&(p)->info.position)
constexpr int FQ_TRANSWALL = 1, EARTHSHAKER_ID = 40, ROBOT_EARTHSHAKER_ID = 58;
constexpr int SOUND_STANDARD_EXPLOSION = 11, SOUND_BADASS_EXPLOSION_WEAPON = 57, SOUNDCLASS_EXPLOSION = 2;
constexpr int MAX_STOLEN_ITEMS = 10;
constexpr int MT_PHYSICS = 1, OBJ_ROBOT = 2, OBJ_PLAYER = 4, CT_MORPH = 1;
fixang FixASin(fix), FixACos(fix), FixAtan2(fix, fix);
struct CAngleVector {
	struct {
		struct {
			fixang p = 0, b = 0, h = 0;
		} coord;
	} v;
};
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	fix Mag() const;
	static const fix Dot(const CFixVector &, const CFixVector &);
	const CFixVector operator*(fix) const;
	const CFixVector operator/(fix) const;
	const CFixVector &operator*=(float);
	CFixVector &Neg();
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	void SetZero()
	{
		Set(0, 0, 0);
	}
	CFixVector &operator-=(const CFixVector &other)
	{
		*this = *this - other;
		return *this;
	}
	static const fix Dist(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	static const fix NormalizedDir(CFixVector &, const CFixVector &, const CFixVector &);
	const CFixVector &operator*=(fix);
	const CFixVector &operator/=(fix);
	const CAngleVector ToAnglesVecNorm() const;
	const CAngleVector ToAnglesVec() const;
	bool IsZero() const
	{
		return !v.coord.x && !v.coord.y && !v.coord.z;
	}
	CFixVector operator-(const CFixVector &other) const
	{
		return { { { v.coord.x - other.v.coord.x, v.coord.y - other.v.coord.y, v.coord.z - other.v.coord.z } } };
	}
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
	CFixVector &operator+=(const CFixVector &other)
	{
		*this = *this + other;
		return *this;
	}
};
struct CObject {
	struct {
		int nId = 32, nSegment = 0, nType = OBJ_PLAYER, movementType = MT_PHYSICS, controlType = 0;
		fix xSize = 65536, xShield = 65536;
		int nFlags = 0, renderType = 0;
		fix xLifeLeft = 65536;
		struct {
			CFixVector vPos;
			struct {
				struct {
					struct {
						CFixVector f;
					} dir;
				} m;
			} mOrient;
		} position;
		CFixVector vLastPos;
	} info;
	struct {
		struct {
			std::int8_t SKIP_AI_COUNT = 0;
		} aiInfo;
		struct {
			struct {
				int nObject = 0;
			} parent;
		} laserInfo;
	} cType;
	struct {
		struct {
			fix mass = 65536;
			CFixVector rotVel, rotThrust, velocity;
			int flags = 0;
		} physInfo;
	} mType;
	fix Size() const
	{
		return info.xSize;
	}
	CFixVector &Position()
	{
		return info.position.vPos;
	}
	bool IsSplashDamageWeapon() const
	{
		return false;
	}
	fix m_xMoveTime = 0, m_xMoveDist = 0;
	CFixVector origin;
	CFixVector Origin() const
	{
		return origin;
	}
	void SetMoveDist(fix value)
	{
		m_xMoveDist = value;
	}
	void SetMoveTime(fix value)
	{
		m_xMoveTime = value;
	}
	void UpdateLife(fix value)
	{
		info.xLifeLeft = value;
	}
	void Die();
	void MoveExplosion();
	bool stationary = false;
	CFixVector applied_rotation;
	bool bump_applied = false;
	bool IsStatic() const
	{
		return stationary;
	}
	void Bump(CObject *, CFixVector, CFixVector, int32_t);
	void Bump(CObject *, CFixVector, int32_t)
	{
		throw std::runtime_error("Unexpected nonstatic bump branch");
	}
	void ApplyForce(CFixVector)
	{
		throw std::runtime_error("Unexpected nonstatic force branch");
	}
	void RelinkToSeg(int)
	{
		throw std::runtime_error("Unexpected sphere separation branch");
	}
	fix created = 0;
	bool attacks = false, guide = false, boss = false;
	bool IsBoss() const
	{
		return boss;
	}
	fix CreationTime() const
	{
		return created;
	}
	bool AttacksRobots() const
	{
		return attacks;
	}
	bool IsGuideBot() const
	{
		return guide;
	}
	bool Indestructible();
	int32_t ApplyDamageToRobot(fix, int32_t);
	CObject *ExplodeSplashDamageWeapon(CFixVector &, CObject *);
	void TurnTowardsVector(CFixVector, fix);
	void ApplyRotForceSource(CFixVector);
	void ApplyRotForce(CFixVector force)
	{
		applied_rotation = force;
		bump_applied = true;
		ApplyRotForceSource(force);
	}
};
struct {
	struct {
		fix xTime = 1092;
	} physicsData;
	struct {
		int nLastHitter = -1;
	} hoardData;
	struct {
		CObject *pConsole = nullptr, *pViewer = nullptr;
		struct {
			int bBoosted = 0;
		} speedBoost[1];
	} objData;
	struct {
		fix xGame = 0, xFrame = 1092;
	} timeData;
} gameData;
struct {
	bool active = false;
	bool Active() const
	{
		return active;
	}
} automap;
struct tRobotInfo {
	bool thief = false, attackType = false;
	int strength = 65536;
} robot_definition;
bool definition_present = true;
CObject *damage_killer = nullptr;
CObject *OBJECT(int)
{
	return damage_killer;
}
tRobotInfo *ROBOTINFO(int)
{
	return definition_present ? &robot_definition : nullptr;
}
int random_value = 0, random_calls = 0;
int RandShort()
{
	++random_calls;
	return random_value;
}
struct {
	struct {
		int nMathFormat = 2;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		bool bD1Mission = false;
		int nDifficultyLevel = 0;
	} app;
	struct {
		int bPerPixelLighting = 0;
	} render;
	struct {
		struct {
			float fSpeed = 1;
		} slowmo[2];
	} gameplay;
} gameStates;
struct tQuadInt {
	int low = 0, high = 0;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected legacy splash math");
}
fix QuadSqrt(int, int)
{
	throw std::runtime_error("Unexpected legacy splash square root");
}
struct CWeaponInfo {
	int nRobotHitAnimation = 3;
} weapon_definition;
CWeaponInfo *WEAPONINFO(int)
{
	return &weapon_definition;
}
#define WI_ImpactSize(i)   65536
#define WI_Strength(i, d)  2621440
#define WI_DamageRadius(i) 655360
#define OBJ_IDX(p)         0
void ShakerRockStuff(CFixVector *)
{
	throw std::runtime_error("Unexpected shaker splash");
}
struct {
	int sound = -1;
	void CreateObjectSound(int s, int, int)
	{
		sound = s;
	}
} audio;
CFixVector explosion_position, explosion_impact;
CObject *CreateSplashDamageExplosion(CObject *weapon, int, CFixVector position, CFixVector impact, fix, int, fix, fix, fix, int)
{
	explosion_position = position;
	explosion_impact = impact;
	return weapon;
}
struct {
	fix distance = 0, damage = 0, force = 0;
	CFixVector impulse;
} radial_result;
bool visible = true;
bool ObjectToObjectVisibility(CObject *, CObject *, int, float)
{
	return visible;
}
using tAIStaticInfo = decltype(CObject::cType.aiInfo);
constexpr int PF_USES_THRUST = 0x40, SHORT_RAND_MAX = 32767;
#include "xl_reference_camera_splash.h"
constexpr int OBJ_WEAPON = 5, OBJ_REACTOR = 9, OBJ_MONSTERBALL = 17, PF_PERSISTENT = 0x20;
#define DBG                  0
#define EGI_FLAG(a, b, c, d) 1
#define Assert(v)                                                      \
	do {                                                               \
		if (!(v)) throw std::runtime_error("Source static assertion"); \
	} while (0)
int CollisionModel()
{
	return 2;
}
int FindSegByPos(CFixVector, int, int, int)
{
	throw std::runtime_error("Unexpected static segment repair");
}
int nMonsterballForces[64]{}, nMonsterballPyroForce = 1;
#include "xl_reference_static_contact.h"
#undef Assert
#undef EGI_FLAG
#undef DBG
void CObject::Die()
{
	info.nFlags |= OF_SHOULD_BE_DEAD;
}
CFixVector vector(const xl_compat::FixedLightPoint &v)
{
	return { { { v[0], v[1], v[2] } } };
}
xl_compat::FixedLightPoint point(const CFixVector &v)
{
	return { v.v.coord.x, v.v.coord.y, v.v.coord.z };
}
} // namespace camera_splash_reference
void camera_splash_source_controls()
{
	using namespace camera_splash_reference;
	unsigned guards = 0;
	for (bool definition : { false, true })
		for (bool exploding : { false, true })
			for (int shields : { -1, 0, 65536 })
				for (int strength : { -1, 0, 65536 })
					for (int created : { 0, 327680 })
						for (int age : { -1, 0, 65535, 65536, 131072 })
							for (bool attacks : { false, true })
								for (int killer_type : { -1, OBJ_PLAYER, OBJ_ROBOT })
									for (bool guide : { false, true }) {
										CObject actor, killer;
										definition_present = definition;
										actor.info.nFlags = exploding ? OF_EXPLODING : 0;
										actor.info.xShield = shields;
										robot_definition.strength = strength;
										actor.created = created;
										gameData.timeData.xGame = created + age;
										actor.attacks = attacks;
										killer.info.nType = killer_type;
										killer.guide = guide;
										damage_killer = killer_type < 0 ? nullptr : &killer;
										const bool expected = actor.ApplyDamageToRobot(65536, killer_type < 0 ? -1 : 1);
										if (expected != xl_compat::robot_damage_allowed(definition, exploding, shields, strength,
										                                                created + age, created, attacks, killer_type == OBJ_ROBOT, guide))
											throw std::runtime_error("Robot damage preconditions differ from pinned source");
										++guards;
									}
	definition_present = true;
	robot_definition = {};
	damage_killer = nullptr;
	std::cout << "Robot damage source preconditions: " << guards << " passed\n";
	unsigned positions = 0, samples = 0;
	for (int weapon_id : { 32, 8, 15, 37, 39 })
		for (int radius : { 0, 1, 65536, 399147 })
			for (int size : { 1, 16384, 65536, 91751 })
				for (const auto relative : { xl_compat::FixedLightPoint{}, xl_compat::FixedLightPoint{ 1, -1, 1 }, xl_compat::FixedLightPoint{ 65536, 0, 0 },
				                             xl_compat::FixedLightPoint{ -72163, 49177, 655359 }, xl_compat::FixedLightPoint{ 399147, 0, 0 } }) {
					CObject weapon, camera;
					weapon.info.nId = weapon_id;
					camera.info.xSize = radius;
					camera.Position() = vector({ 300001, -200003, 100007 });
					weapon.info.xSize = size;
					auto impact = camera.Position() + vector(relative);
					const auto initial = point(impact);
					weapon.ExplodeSplashDamageWeapon(impact, &camera);
					const auto result = xl_compat::camera_splash_impact(initial, point(camera.Position()), radius, size);
					if (result != point(impact) || result != point(explosion_position) || result != point(explosion_impact) || audio.sound != 11)
						throw std::runtime_error("Camera splash impact differs from pinned full weapon explosion function");
					++positions;
				}
	for (int radius : { 1, 65536, 655360, 3276800 })
		for (int strength : { 1, 65536, 2621440, 9175040 })
			for (int x : { -655360, -65535, -1, 0, 1, 65535, 655360 })
				for (int y : { -327681, -1, 0, 1, 327681 })
					for (int z : { -65536, 0, 65536 }) {
						CObject actor;
						actor.Position() = vector({ x, y, z });
						visible = true;
						const bool inside = radial(&actor, vector({ 0, 0, 0 }), strength, radius, strength);
						const auto result = xl_compat::camera_splash_sample({ x, y, z }, {}, strength, radius, strength);
						if (inside != result.inside || (inside && (result.distance != radial_result.distance || result.damage != radial_result.damage ||
						                                           result.force != radial_result.force || result.impulse != point(radial_result.impulse))))
							throw std::runtime_error("Camera splash radial geometry differs from pinned explosion loop");
						visible = false;
						if (radial(&actor, vector({ 0, 0, 0 }), strength, radius, strength)) throw std::runtime_error("Invisible splash target accepted");
						++samples;
					}
	const auto center = xl_compat::camera_splash_impact({ 1, 2, 3 }, { 1, 2, 3 }, 65536, 16384);
	if (center != xl_compat::FixedLightPoint{ 2, 3, 4 }) throw std::runtime_error("Source zero-divisor splash convention changed");
	std::cout << "Pinned camera splash: " << positions << " full weapon impact calls and " << samples
	          << " radial geometry/visibility cases; source math macros, target/weapon size distinction and zero-divisor convention passed\n";
}

void camera_splash_rotation_runtime_controls(const char *path)
{
	using namespace camera_splash_reference;
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Missing native camera rotation receipt");
	unsigned count = 0;
	std::array<int, 25> row{};
	while (input >> row[0]) {
		for (unsigned i = 1; i < row.size(); ++i)
			if (!(input >> row[i])) throw std::runtime_error("Truncated camera rotation receipt");
		CObject actor;
		actor.info.nType = row[0] ? OBJ_ROBOT : OBJ_PLAYER;
		actor.info.movementType = row[1] ? MT_PHYSICS : 0;
		actor.info.controlType = row[2] ? CT_MORPH : 0;
		actor.mType.physInfo.mass = row[3];
		gameData.physicsData.xTime = row[4];
		actor.cType.aiInfo.SKIP_AI_COUNT = row[5];
		robot_definition = { bool(row[6]), bool(row[7]) };
		gameData.objData.pConsole = row[8] ? &actor : nullptr;
		automap.active = row[9];
		random_value = row[10];
		random_calls = 0;
		actor.info.position.mOrient.m.dir.f = vector({ row[14], row[15], row[16] });
		actor.mType.physInfo.rotVel = vector({ row[17], row[18], row[19] });
		actor.ApplyRotForce(vector({ row[11], row[12], row[13] }));
		if (point(actor.mType.physInfo.rotVel) != xl_compat::FixedLightPoint{ row[20], row[21], row[22] } ||
		    actor.cType.aiInfo.SKIP_AI_COUNT != row[23] || random_calls != row[24])
			throw std::runtime_error("Native camera rotation differs from pinned source at row " + std::to_string(count));
		++count;
	}
	if (!input.eof() || count < 1000) throw std::runtime_error("Incomplete camera rotation receipt");
	std::cout << "Native camera splash rotation: " << count << " source comparisons passed\n";
}

void camera_explosion_motion_runtime_controls(const char *path)
{
	using namespace camera_splash_reference;
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Missing native camera explosion motion receipt");
	std::array<int, 15> row{};
	unsigned count = 0;
	while (input >> row[0]) {
		for (unsigned i = 1; i < row.size(); ++i)
			if (!(input >> row[i])) throw std::runtime_error("Truncated explosion motion receipt");
		CObject effect, viewer;
		effect.origin = effect.Position() = vector({ row[0], row[1], row[2] });
		viewer.Position() = vector({ row[3], row[4], row[5] });
		gameData.objData.pViewer = &viewer;
		effect.info.xSize = row[6];
		effect.info.xLifeLeft = row[7];
		effect.info.renderType = RT_FIREBALL;
		initialize_motion(&effect, row[14]);
		effect.info.xLifeLeft = row[8];
		effect.MoveExplosion();
		if (point(effect.Position()) != xl_compat::FixedLightPoint{ row[9], row[10], row[11] } ||
		    effect.info.xLifeLeft != row[12] || bool(effect.info.nFlags & OF_SHOULD_BE_DEAD) != bool(row[13]))
			throw std::runtime_error("Native explosion motion differs from source at row " + std::to_string(count));
		++count;
	}
	if (!input.eof() || count < 100) throw std::runtime_error("Incomplete explosion motion receipt");
	std::cout << "Native camera explosion motion: " << count << " source comparisons passed\n";
}

void camera_flash_robot_source_controls()
{
	using namespace camera_splash_reference;
	unsigned cases = 0;
	for (int frame : { 0, 546, 1092, 2184, 32768, 65536 })
		for (int flash : { 0, 1, 4, 8, 16 })
			for (bool boss : { false, true })
				for (int skip : { -128, -1, 0, 60, 127 })
					for (int explosion_skip : { -1, 0, 60, 127 })
						for (int sample : { 0, 16383, 32767 })
							for (const auto force : { xl_compat::FixedLightPoint{}, xl_compat::FixedLightPoint{ 65536, 0, 0 }, xl_compat::FixedLightPoint{ -983041, 65537, 131071 } }) {
								CObject actor, explosion;
								actor.boss = boss;
								actor.cType.aiInfo.SKIP_AI_COUNT = std::int8_t(skip);
								explosion.cType.aiInfo.SKIP_AI_COUNT = std::int8_t(explosion_skip);
								actor.mType.physInfo.rotThrust = vector({ 7, -8, 9 });
								gameData.timeData.xFrame = frame;
								random_value = sample;
								random_calls = 0;
								flash_robot(&actor, &explosion, vector(force), flash);
								int actual_skip = skip, calls = 0;
								xl_compat::FixedLightPoint thrust{ 7, -8, 9 };
								bool applied = xl_compat::camera_flash_robot(actual_skip, thrust, force, flash, boss, explosion_skip, frame,
								                                             [&] { ++calls; return sample; });
								if (actual_skip != actor.cType.aiInfo.SKIP_AI_COUNT || thrust != point(actor.mType.physInfo.rotThrust) ||
								    applied != bool(actor.mType.physInfo.flags & PF_USES_THRUST) || calls != random_calls)
									throw std::runtime_error("Camera flash robot stun differs from pinned source");
								++cases;
							}
	std::cout << "Camera flash robot source stun: " << cases << " passed\n";
}

void static_contact_source_controls()
{
	using namespace camera_splash_reference;
	unsigned cases = 0;
	for (int type : { OBJ_PLAYER, OBJ_WEAPON })
		for (int mass : { 0, 1, 3, 32768, 65536, 11 * 65536, 400 * 65536 })
			for (int actor_mass : { 0, 1, 3, 32768, 65536, 11 * 65536 })
				for (int speed : { -100003, -1, 0, 1, 100003 })
					for (int profile = 0; profile < 3; ++profile)
						for (bool persistent : { false, true })
							for (bool boosted : { false, true }) {
								CObject fixed, actor;
								fixed.stationary = true;
								fixed.info.nType = OBJ_ROBOT;
								fixed.info.movementType = 0;
								fixed.info.position.vPos = vector({ 12345, -67890, 13579 });
								fixed.mType.physInfo.mass = mass;
								actor.info.nType = type;
								actor.info.position.vPos = vector({ -156789, 3357, 13579 });
								actor.info.position.mOrient.m.dir.f = vector({ 0, 0, 65536 });
								actor.mType.physInfo.mass = actor_mass;
								actor.mType.physInfo.velocity = vector({ speed, speed / 3, -speed / 5 });
								actor.mType.physInfo.flags = persistent ? PF_PERSISTENT : 0;
								gameData.objData.pConsole = &actor;
								gameData.objData.speedBoost[0].bBoosted = boosted;
								gameData.physicsData.xTime = 1092;
								automap.active = false;
								const std::optional<xl_compat::FixedLightPoint> normal = profile ? std::optional<xl_compat::FixedLightPoint>(profile == 1 ? xl_compat::FixedLightPoint{} : xl_compat::FixedLightPoint{ 65536, 0, 0 }) : std::nullopt;
								CFixVector source_normal = normal ? vector(*normal) : CFixVector{}, hit;
								const auto result = xl_compat::static_contact(point(fixed.Position()), mass, point(actor.Position()), point(actor.mType.physInfo.velocity), actor_mass,
								                                              type == OBJ_PLAYER, persistent, type == OBJ_PLAYER && boosted, normal);
								BumpTwoObjects(&fixed, &actor, 0, hit, normal ? &source_normal : nullptr);
								if (result.apply != actor.bump_applied || result.velocity != point(actor.mType.physInfo.velocity) ||
								    (result.apply && result.rotation_force != point(actor.applied_rotation)) || fixed.bump_applied || !fixed.mType.physInfo.velocity.IsZero())
									throw std::runtime_error("Static bump differs from full pinned BumpTwoObjects at case " + std::to_string(cases));
								++cases;
							}
	gameData.objData.pConsole = nullptr;
	gameData.objData.speedBoost[0].bBoosted = 0;
	std::cout << "Pinned static contacts: " << cases << " full bump calls, zero/unequal masses, persistent/boost gates and raw rotation forces passed\n";
}
