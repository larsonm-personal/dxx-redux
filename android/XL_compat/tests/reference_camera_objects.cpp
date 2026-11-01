#include "camera_object_state.h"
#include "camera_contact.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace camera_object_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN return;
#define I2X(v) ((v) * 65536)
constexpr int OBJ_CAMBOT = 16, OBJ_EFFECT = 20, CT_CAMERA = 18, CT_WAYPOINT = 17, CT_NONE = 0, MT_NONE = 0;
constexpr int RT_POLYOBJ = 1, AIB_STILL = 128, AIM_IDLING = 0, IMMORTAL_TIME = 0x3fffffff;
fix FixDiv(fix a, fix b)
{
	return fix(std::int64_t(a) * 65536 / b);
}
struct tRobotInfo {
	int nModel = 0, attackType = 0, containsId = 0, containsCount = 0, containsProb = 0, containsType = 0, scoreValue = 0;
	fix strength = 0, mass = 0, drag = 0;
	int seeSound = 0, attackSound = 0, clawSound = 0, tauntSound = 0, behavior = 0, aim = 0;
	int turnTime[5]{}, xMaxSpeed[5]{}, circleDistance[5]{}, nRapidFireCount[5]{};
};
struct CObject {
	struct {
		int nType = OBJ_CAMBOT, nId = 0, renderType = RT_POLYOBJ, controlType = 0, movementType = 0;
		fix xSize = 0;
	} info;
	struct {
		struct {
			int nModel = 0;
		} polyObjInfo;
	} rType;
	fix life = 0;
	int index = 0;
	int Index() const
	{
		return index;
	}
	int ModelId() const
	{
		return rType.polyObjInfo.nModel;
	}
	fix ModelRadius(int);
	void AdjustSize(int32_t i = 0, fix scale = 0);
	void SetSize(fix size)
	{
		info.xSize = size;
	}
	void SetLife(fix value)
	{
		life = value;
	}
};
std::array<CObject, 3> objects;
// Pinned linked static-list classification includes cambots, unlike its unused fallback
#define FORALL_STATIC_OBJS(p)   \
	for (auto &entry : objects) \
		if (entry.info.nType == OBJ_CAMBOT && ((p) = &entry))
struct {
	struct {
		bool bD1Mission = false;
	} app;
} gameStates;
struct {
	struct {
		tRobotInfo info[1][64];
		int nCamBotId = 42, nCamBotModel = 60;
	} botData;
	struct {
		struct {
			int mode = 0;
		} localInfo[3];
	} aiData;
	struct {
		struct Model {
			fix radii[2]{};
			fix Rad(int i) const
			{
				return radii[i] ? radii[i] : radii[!i];
			}
		} polyModels[1][64];
	} modelData;
} gameData;
#include "xl_reference_camera_objects.h"
} // namespace camera_object_reference

namespace camera_rotation_reference
{
void controls();
}
namespace camera_force_reference
{
void controls();
}
namespace camera_gauss_reference
{
void controls();
}
void camera_splash_source_controls();

void camera_object_source_controls()
{
	using namespace camera_object_reference;
	camera_rotation_reference::controls();
	camera_force_reference::controls();
	camera_gauss_reference::controls();
	camera_splash_source_controls();
	unsigned cases = 0;
	for (int original_model : { 0, 1, 60 })
		for (int render : { 0, 1 })
			for (int original_size : { 0, 12345, 399147 })
				for (int old_base : { 0, 65536, 399147 })
					for (int old_custom : { 0, 116018 }) {
						gameData.modelData.polyModels[0][original_model].radii[0] = old_base;
						gameData.modelData.polyModels[0][original_model].radii[1] = old_custom;
						for (int i = 0; i < 3; ++i) {
							objects[i] = {};
							objects[i].index = i;
							objects[i].info = { OBJ_CAMBOT, 7, render, 1, 1, original_size };
							objects[i].rType.polyObjInfo.nModel = original_model;
							objects[i].life = 123;
							gameData.aiData.localInfo[i].mode = 9;
						}
						xl_compat::CameraObjectState portable{ 7, 1, 1, original_model, 9, original_size, 123 };
						const int old_radius = render == RT_POLYOBJ ? (old_base ? old_base : old_custom) : original_size;
						xl_compat::initialize_camera_object(portable, 42, 60, old_radius);
						InitCamBots(0);
						for (const auto &source : objects) {
							if (source.info.nId != portable.id || source.info.controlType != portable.control || source.info.movementType != portable.movement ||
							    source.rType.polyObjInfo.nModel != portable.model || source.info.xSize != portable.size || source.life != portable.life ||
							    gameData.aiData.localInfo[source.index].mode != portable.ai_mode || source.info.nType != OBJ_CAMBOT || source.info.renderType != render)
								throw std::runtime_error("Camera initialization differs from pinned InitCamBots/AdjustSize");
							++cases;
						}
					}
	std::cout << "Pinned camera initialization: " << cases << " object states; old-model size before addon assignment, zero/custom radius fallback, lifetime, control, movement and AI mode passed\n";
}

namespace camera_rotation_reference
{
using fix = std::int32_t;
using fixang = std::int16_t;
using time_t = std::int64_t;
constexpr int AIB_NORMAL = 129;
struct CAngleVector {
	struct {
		struct {
			fixang p = 0, b = 0, h = 0;
		} coord;
	} v;
};
// Observe the original-orientation multiply and repair call, not matrix arithmetic
struct CFixMatrix {
	int heading = 0, original = 0, repairs = 0;
	static CFixMatrix Create(const CAngleVector &angle)
	{
		if (angle.v.coord.p || angle.v.coord.b) throw std::runtime_error("Unexpected camera pitch/bank");
		return { angle.v.coord.h, 0, 0 };
	}
	CFixMatrix operator*(const CFixMatrix &rhs) const
	{
		return { rhs.heading, original, 0 };
	}
	void CheckAndFix()
	{
		++repairs;
	}
};
struct CObject {
	struct {
		struct {
			int behavior = 128;
		} aiInfo;
	} cType;
	struct {
		struct {
			fix brakes = 73;
		} physInfo;
	} mType;
	struct {
		struct {
			CFixMatrix mOrient{ 27, 19, 0 };
		} position;
	} info;
};
struct {
	struct {
		time_t nSDLTicks[1]{};
	} app;
} gameStates;
struct {
	struct {
		struct {
			int nSpeed = 5000;
		} cameras;
	} render;
} options;
auto *gameOpts = &options;
struct CCamera {
	struct {
		CObject *pObj = nullptr;
		fixang curAngle = 0, curDelta = 0;
		time_t t0 = 0;
		CFixMatrix orient{ 0, 61, 0 };
	} m_data;
	void Rotate();
};
#include "xl_reference_camera_rotation.h"

void controls()
{
	unsigned cases = 0, rotations = 0;
	auto compare = [&](CCamera &source, CObject &object, xl_compat::CameraRotation &portable, fix &brakes) {
		const auto before = object.info.position.mOrient;
		const bool changed = portable.advance(source.m_data.pObj != nullptr, object.cType.aiInfo.behavior,
		                                      gameStates.app.nSDLTicks[0], options.render.cameras.nSpeed, brakes);
		source.Rotate();
		if (source.m_data.curAngle != portable.angle || source.m_data.curDelta != portable.delta ||
		    source.m_data.t0 != portable.t0 || object.mType.physInfo.brakes != brakes)
			throw std::runtime_error("Camera heading differs from pinned controller");
		const auto &after = object.info.position.mOrient;
		if (changed ? (after.heading != portable.angle || after.original != 61 || after.repairs != 1) : (after.heading != before.heading || after.original != before.original || after.repairs != before.repairs))
			throw std::runtime_error("Camera rotation matrix call ordering differs");
		++cases;
		rotations += changed;
	};
	for (bool attached : { false, true })
		for (int behavior : { 128, 129, 135 })
			for (int speed : { 1000, 1999, 2000, 5000, 10000, 183000 })
				for (int angle : { -32768, -8374, -8193, -8192, -8191, 0, 8191, 8192, 8193, 8374, 32767 })
					for (int delta : { -182, -36, 0, 36, 182 })
						for (time_t t0 : { -1, 0, 100 })
							for (time_t now : { 0, 10, 11, 100, 110, 111, 100000 }) {
								CObject object;
								object.cType.aiInfo.behavior = behavior;
								CCamera source;
								source.m_data.pObj = attached ? &object : nullptr;
								source.m_data.curAngle = fixang(angle);
								source.m_data.curDelta = fixang(delta);
								source.m_data.t0 = t0;
								xl_compat::CameraRotation portable{ fixang(angle), fixang(delta), t0 };
								fix brakes = 73;
								gameStates.app.nSDLTicks[0] = now;
								options.render.cameras.nSpeed = speed;
								compare(source, object, portable, brakes);
							}
	CObject object;
	object.cType.aiInfo.behavior = AIB_NORMAL;
	CCamera source;
	source.m_data.pObj = &object;
	xl_compat::CameraRotation portable;
	fix brakes = 73;
	for (int frame = 0; frame < 30000; ++frame) {
		gameStates.app.nSDLTicks[0] = frame / 3;
		options.render.cameras.nSpeed = (frame / 2000 % 10 + 1) * 1000;
		compare(source, object, portable, brakes);
	}
	for (int invalid : { 0, 999, -1000 }) {
		xl_compat::CameraRotation state;
		fix saved = 73;
		bool rejected = false;
		try {
			state.advance(true, 129, 11, invalid, saved);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected || saved != 73 || state.angle || state.delta)
			throw std::runtime_error("Invalid camera speed was not rejected transactionally");
	}
	std::cout << "Pinned camera heading: " << cases << " states/frames, " << rotations
	          << " rotations; still/static/no-owner gates, retained t0, signed limits, speed changes and matrix call ordering passed\n";
}
} // namespace camera_rotation_reference

namespace camera_force_reference
{
using fix = std::int32_t;
#define DBG          0
#define X2F(v)       (float(v) / 65536.0f)
#define SPECTATOR(p) ((p)->spectator)
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	const CFixVector &operator*=(float);
	const CFixVector operator*(fix) const;
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	const CFixVector &operator+=(const CFixVector &other)
	{
		v.coord.x += other.v.coord.x;
		v.coord.y += other.v.coord.y;
		v.coord.z += other.v.coord.z;
		return *this;
	}
};
constexpr int MT_PHYSICS = 1;
struct CObject {
	bool spectator = false;
	struct {
		int movementType = MT_PHYSICS;
	} info;
	struct {
		struct {
			fix mass = 65536;
			CFixVector velocity;
		} physInfo;
	} mType;
	int Index() const
	{
		return 0;
	}
	CFixVector &Velocity()
	{
		return mType.physInfo.velocity;
	}
	void ApplyForce(CFixVector);
};
struct {
	bool active = false;
	bool Active() const
	{
		return active;
	}
} automap;
struct {
	struct {
		CObject *pConsole = nullptr;
		struct {
			int bBoosted = 0;
		} speedBoost[1];
	} objData;
} gameData;
#include "xl_reference_camera_force.h"
void controls()
{
	unsigned cases = 0, rounding = 0;
	for (fix mass : { 0, 1, 3, 32768, 65536, 98317, 262144 })
		for (fix velocity : { -100001, -65536, -17, -1, 0, 1, 17, 65536, 100001 })
			for (bool console : { false, true })
				for (bool map : { false, true })
					for (bool spectator : { false, true })
						for (int boosted : { 0, 1 }) {
							CObject object;
							object.spectator = spectator;
							object.mType.physInfo.mass = mass;
							object.mType.physInfo.velocity.v.coord = { velocity, velocity / 3, -velocity };
							const auto force = object.Velocity() * (-2 * mass);
							automap.active = map;
							gameData.objData.pConsole = console ? &object : nullptr;
							gameData.objData.speedBoost[0].bBoosted = boosted;
							const auto result = xl_compat::camera_reflect_velocity({ velocity, velocity / 3, -velocity }, mass,
							                                                       spectator || (console && (map || boosted)));
							object.ApplyForce(force);
							const auto &actual = object.Velocity().v.coord;
							if (actual.x != result[0] || actual.y != result[1] || actual.z != result[2])
								throw std::runtime_error("Camera reflection differs from release source ApplyForce");
							if (mass && !spectator && !(console && (map || boosted)) && result[0] != -velocity) ++rounding;
							++cases;
						}
	if (!rounding) throw std::runtime_error("Camera force control did not exercise fixed/float rounding");
	std::cout << "Pinned camera force: " << cases << " release ApplyForce states, " << rounding
	          << " non-exact reflection cases; zero mass, spectator, automap and console speed boost passed\n";
}
} // namespace camera_force_reference

namespace camera_gauss_reference
{
using fix = std::int32_t;
constexpr int SHORT_RAND_MAX = 32767, GAUSS_ID = 32, PF_USES_THRUST = 64;
struct tAIStaticInfo {
	int SKIP_AI_COUNT = 0;
};
struct tRobotInfo {
	bool companion = false, bossFlag = false;
};
struct CObject {
	struct {
		tAIStaticInfo aiInfo;
	} cType;
	struct {
		struct {
			struct {
				struct {
					struct {
						fix x = 0, y = 0, z = 0;
					} coord;
				} v;
			} rotThrust;
			int flags = 0;
		} physInfo;
	} mType;
};
struct {
	int nId = GAUSS_ID;
} info;
struct {
	struct {
		fix xFrame = 0;
	} timeData;
} gameData;
std::array<int, 3> random_values{};
int random_calls = 0;
int RandShort()
{
	return random_values.at(std::size_t(random_calls++));
}
#include "xl_reference_camera_gauss.h"
void controls()
{
	unsigned cases = 0;
	for (int frame : { 512, 546, 1092, 3276, 6553, 65536 })
		for (int skip : { 0, 1, 9, 10, 59, 60, 119, 120, 126 })
			for (int first : { 0, 1, 16383, 16384, 32767 }) {
				CObject object;
				object.cType.aiInfo.SKIP_AI_COUNT = skip;
				object.mType.physInfo.rotThrust.v.coord = { 71, -51, 91 };
				object.mType.physInfo.flags = 3;
				gameData.timeData.xFrame = frame;
				random_values = { first, 32767 - first, 16384 };
				random_calls = 0;
				spin(&object);
				int portable_skip = skip, calls = 0;
				std::array<std::int32_t, 3> thrust{ 71, -51, 91 };
				const bool changed = xl_compat::camera_gauss_spin(portable_skip, frame, thrust, [&] { return random_values.at(std::size_t(calls++)); });
				const auto &actual = object.mType.physInfo.rotThrust.v.coord;
				if (object.cType.aiInfo.SKIP_AI_COUNT != portable_skip || actual.x != thrust[0] || actual.y != thrust[1] || actual.z != thrust[2] ||
				    calls != random_calls || calls != (changed ? 3 : 0) || object.mType.physInfo.flags != (3 | (changed ? PF_USES_THRUST : 0)))
					throw std::runtime_error("Camera Gauss state or signed RNG differs from pinned collision branch");
				++cases;
			}
	std::cout << "Pinned camera Gauss: " << cases << " frame/skip/RNG states; signed draw convention, increment-before-thrust, saturation gate and exact three draws passed\n";
}
} // namespace camera_gauss_reference
