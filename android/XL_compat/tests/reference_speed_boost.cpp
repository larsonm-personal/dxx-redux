#include "reference_motion.h"
#include <vector>
#define COMPETITION    0
#define IsMultiGame    0
#define PF_USES_THRUST 64
#define TF_SET_ORIENT  32
#define ENTER(a, b)
#define RETURN               return;
#define EGI_FLAG(a, b, c, d) 10
constexpr fix FT = I2X(1) / 64;
struct tSpeedBoostData {
	int bBoosted = 0;
	CFixVector vVel, vMinVel, vMaxVel, vSrc, vDest;
};
struct CPhysSimData {
	fix xSimTime = 0;
	int bSpeedBoost = 0;
	tSpeedBoostData speedBoost;
};
struct {
	struct {
		tSpeedBoostData speedBoost[2];
		CObject *pConsole = &object;
	} objData;
} gameData;
struct {
	int nSpeedBoost = 10;
} extraGameInfo[1];
struct {
	fix verticalThrustTime = 0, forwardThrustTime = 0, sidewaysThrustTime = 0;
} controls[1];
struct CWall {
	int nSegment = 1, nSide = 0;
} wall;
bool parent = false;
CWall *TriggerParentWall(int)
{
	return parent ? &wall : nullptr;
}
struct CTrigger {
	int m_nLinks = 1, flags = 0;
	fix value = 1;
	int m_segments[1]{}, m_sides[1]{};
	int Index() const
	{
		return 0;
	}
	int Flagged(int bit) const
	{
		return flags & bit;
	}
	fix GetValue() const
	{
		return value;
	}
	void DoSpeedBoost(int16_t);
	void StopSpeedBoost(int16_t);
};
#include "xl_reference_speed_boost.h"

static void read_vector(std::istream &input, CFixVector &value)
{
	input >> value.v.coord.x >> value.v.coord.y >> value.v.coord.z;
}
static void append(std::vector<fix> &output, const CFixVector &value)
{
	for (fix component : { value.v.coord.x, value.v.coord.y, value.v.coord.z }) output.push_back(component);
}
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected speed boost samples");
		std::ifstream input(argv[1]);
		int count = 0;
		char kind;
		while (input >> kind) {
			std::vector<fix> output;
			if (kind == 'A') {
				CTrigger action;
				int mode, ticks, links;
				input >> action.value >> speedBoostSpeed >> mode >> action.flags >> ticks >> links;
				action.m_nLinks = links;
				parent = mode != 0;
				segments[0].child = mode == 2 ? 1 : -1;
				segments[1].child = mode == 2 ? 0 : -1;
				read_vector(input, segments[0].normal);
				read_vector(input, segments[1].center);
				read_vector(input, segments[0].center);
				read_vector(input, object.mType.physInfo.velocity);
				for (auto *vector : { &object.info.position.mOrient.m.dir.r, &object.info.position.mOrient.m.dir.u, &object.info.position.mOrient.m.dir.f }) read_vector(input, *vector);
				gameData.objData.speedBoost[0] = {};
				gameStates.gameplay = {};
				action.DoSpeedBoost(0);
				for (int tick = 0; tick < ticks && gameStates.gameplay.nDirSteps; ++tick)
					TriggerSetObjOrient(0, -1, -1, 0, gameStates.gameplay.nDirSteps--);
				const auto &state = gameData.objData.speedBoost[0];
				output = { state.bBoosted, speedBoostSpeed, gameStates.gameplay.nDirSteps };
				append(output, state.vVel);
				append(output, state.vMinVel);
				append(output, state.vMaxVel);
				append(output, object.mType.physInfo.velocity);
				for (const auto *vector : { &object.info.position.mOrient.m.dir.r, &object.info.position.mOrient.m.dir.u, &object.info.position.mOrient.m.dir.f }) append(output, *vector);
			} else if (kind == 'D') {
				CPhysSimData simulation;
				simulation.speedBoost = gameData.objData.speedBoost[0];
				simulation.bSpeedBoost = simulation.speedBoost.bBoosted;
				input >> simulation.xSimTime >> object.mType.physInfo.mass >> object.mType.physInfo.drag;
				object.mType.physInfo.flags = PF_USES_THRUST;
				read_vector(input, object.mType.physInfo.thrust);
				read_vector(input, object.mType.physInfo.velocity);
				object.ProcessDrag(simulation);
				append(output, object.mType.physInfo.velocity);
			} else throw std::runtime_error("Unknown sample kind");
			for (std::size_t field = 0; field < output.size(); ++field) {
				fix actual;
				if (!(input >> actual)) throw std::runtime_error("Truncated sample");
				if (actual != output[field]) throw std::runtime_error("Boost reference mismatch at sample " + std::to_string(count) + " field " + std::to_string(field) + " expected " + std::to_string(output[field]) + " actual " + std::to_string(actual));
			}
			++count;
		}
		if (count < 500) throw std::runtime_error("Insufficient boost samples");
		std::cout << count << " extracted XL boost activation, orientation and drag samples passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
