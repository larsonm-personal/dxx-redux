#include "reference_motion.h"
#include "xl_reference_teleport.h"
#include <sstream>
#define PF_USES_THRUST       64
#define EGI_FLAG(a, b, c, d) 10
#define RETURN               return;
constexpr fix FT = I2X(1) / 64;
#define ENTER(a, b)
#define RETVAL(value) return (value);
constexpr int OBJ_PLAYER = 4, OBJ_ROBOT = 2, EARTHSHAKER_MEGA_ID = 54, ROBOT_SHAKER_MEGA_ID = 61;
constexpr int IsMultiGame = 0;
fix FixMulDiv(fix value, fix scale, fix divisor)
{
	return static_cast<fix>(std::int64_t(value) * scale / divisor);
}
float MissileSpeedScale(CObject *)
{
	return 1.0f;
}
struct Boost {
	int bBoosted = 0;
	CFixVector vVel, vMinVel, vMaxVel;
};
struct {
	struct {
		fix xTime = 0;
		int nSegments = 0;
	} physicsData;
	struct {
		Boost speedBoost[2];
		CObject *pConsole = &objects[1];
	} objData;
} gameData;
struct {
	int bFluidPhysics = 1, nSpeedScale = 0;
} extraGameInfo[1];
struct CPhysSimData {
	fix xOldSimTime = 0, xMovedTime = 0, xMovedDist = 0, xAttemptedDist = 0, xSimTime = 0;
	CFixVector vNewPos, vHitPos, vOffset, vMoved, velocity, vOldPos, vStartPos;
	int bUseHitbox = 0, bStopped = 0, bBounced = 0, bIgnoreObjFlag = 0, nTries = 0, bInitialize = 0, bSpeedBoost = 0, bGetPhysSegs = 0, bScaleSpeed = 0;
	int nObject = 0, nOldSeg = 0, nStartSeg = 0, xTimeScale = 0;
	Boost speedBoost;
	CObject *pObj = nullptr;
	void Setup();
};
#include "xl_reference_water_actors.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected water motion samples");
		std::ifstream input(argv[1]);
		int type, wet, count = 0;
		fix time, values[6];
		std::string line;
		while (std::getline(input, line)) {
			std::istringstream row(line);
			if (!(row >> type >> wet >> time)) throw std::runtime_error("Truncated motion row");
			for (auto &value : values)
				if (!(row >> value)) throw std::runtime_error("Truncated water motion sample");
			int flags = 0;
			fix mass = I2X(1), drag = 0, thrust[3]{}, final_velocity[3]{};
			bool forces = bool(row >> flags);
			if (forces && !(row >> mass >> drag >> thrust[0] >> thrust[1] >> thrust[2] >> final_velocity[0] >> final_velocity[1] >> final_velocity[2])) throw std::runtime_error("Truncated motion force row");
			object.mType.physInfo.flags = flags;
			object.mType.physInfo.mass = mass;
			object.mType.physInfo.drag = drag;
			object.mType.physInfo.thrust.Set(thrust[0], thrust[1], thrust[2]);
			object.info.nType = type;
			object.info.nSegment = 0;
			object.mType.physInfo.velocity.Set(values[0], values[1], values[2]);
			segment.water = (wet & 1) != 0;
			segment.lava = (wet & 2) != 0;
			gameData.physicsData.xTime = time;
			CPhysSimData state;
			state.Setup();
			if (forces) object.ProcessDrag(state);
			object.UpdateOffset(state);
			if (forces) {
				const auto &v = object.mType.physInfo.velocity.v.coord;
				const fix expected_velocity[] = { v.x, v.y, v.z };
				for (int axis = 0; axis < 3; ++axis)
					if (final_velocity[axis] != expected_velocity[axis]) throw std::runtime_error("Fluid drag differs at sample " + std::to_string(count) + " axis " + std::to_string(axis) + " expected " + std::to_string(expected_velocity[axis]) + " actual " + std::to_string(final_velocity[axis]));
			}
			const fix expected[] = { state.vOffset.v.coord.x, state.vOffset.v.coord.y, state.vOffset.v.coord.z };
			for (int axis = 0; axis < 3; ++axis)
				if (values[3 + axis] != expected[axis]) throw std::runtime_error("Water motion differs at sample " + std::to_string(count) + " axis " + std::to_string(axis) + " expected " + std::to_string(expected[axis]) + " actual " + std::to_string(values[3 + axis]));
			++count;
		}
		if (count < 72) throw std::runtime_error("Insufficient water actor samples");
		std::cout << count << " extracted-reference water/dry actor offsets passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
