// Storage adapters for pinned XL assignment; expected mappings come from extracted code
#include "inventory.h"
#include "probe.h"
#include "producer_layout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#define SEGMENT_FUNC_ROBOTMAKER       4
#define SEGMENT_FUNC_EQUIPMAKER       11
#define SEGMENT_FUNC_NONE             0
#define SEGMENT_FUNC_FUELCENTER       1
#define SEGMENT_FUNC_REPAIRCENTER     2
#define SEGMENT_FUNC_REACTOR          3
#define MAX_FUEL_CENTERS              500
#define OBJ_PRODUCER_HP_DEFAULT       I2X(500)
#define OBJ_PRODUCER_INTERVAL_DEFAULT I2X(5)
#define ENTER(...)
#define RETVAL(value)          return (value);
#define RETURN                 return -1;
#define I2X(value)             ((value) * 65536)
#define DIFFICULTY_LEVEL_COUNT 5
#define OBJECT_PRODUCER_LIFE   I2X(30 - 2 * gameStates.app.nDifficultyLevel)
#define SINGLE_LIGHT_ID        0
#define OBJ_ROBOT              2
#define OBJ_REACTOR            9
#define OBJ_PLAYER             4
#define MT_PHYSICS             1
#define SEGMENT_SIDE_COUNT     6
#define WID_PASSABLE_FLAG      1
#define N_LOCALPLAYER          0
#define SOUND_PLAYER_GOT_HIT   1
#define SOUND_ROBOT_HIT        2
#define ANIM_PLAYER_HIT        0
#define SPECTATOR(object)      false
#define X2F(value)             (float(value) / 65536.0f)
using fix = std::int32_t;
struct tRobotInfo {
	fix strength = 0;
	bool thief = false, companion = false;
	int bossFlag = 0;
	int nExp1VClip = 0;
};
struct {
	int nCurrentLevel = 1;
} missionManager;
fix ReactorStrength()
{
	throw std::runtime_error("Unqualified reactor shield branch");
}
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	fix Mag() const;
	static fix Normalize(CFixVector &vector);
	static const CFixVector Random();
	const CFixVector &operator*=(fix value);
	const CFixVector &operator*=(float value);
	void SetZero()
	{
		v.coord = {};
	}
	bool IsZero() const
	{
		return !v.coord.x && !v.coord.y && !v.coord.z;
	}
	CFixVector operator-(const CFixVector &other) const
	{
		return { { { v.coord.x - other.v.coord.x, v.coord.y - other.v.coord.y, v.coord.z - other.v.coord.z } } };
	}
	CFixVector operator*(fix scale) const
	{
		return { { { fix(std::int64_t(v.coord.x) * scale / 65536), fix(std::int64_t(v.coord.y) * scale / 65536), fix(std::int64_t(v.coord.z) * scale / 65536) } } };
	}
	CFixVector &operator+=(const CFixVector &other)
	{
		v.coord.x += other.v.coord.x;
		v.coord.y += other.v.coord.y;
		v.coord.z += other.v.coord.z;
		return *this;
	}
};
struct {
	struct {
		int nMathFormat = 2;
	} render;
} options;
auto *gameOpts = &options;
struct tQuadInt {
	int low, high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unqualified fixed math format");
}
fix QuadSqrt(int, int)
{
	throw std::runtime_error("Unqualified fixed math format");
}
struct {
	struct {
		bool bD1Mission = false;
		int nDifficultyLevel = 0;
	} app;
} gameStates;
struct CObject {
	struct {
		int nType = 0, nId = 0, nSegment = 0, movementType = MT_PHYSICS;
		fix xSize = 65536;
		struct {
			CFixVector vPos{};
		} position;
	} info;
	struct {
		struct {
			fix mass = 65536;
			CFixVector velocity;
		} physInfo;
	} mType;
	fix damage = 0;
	int32_t CollidePlayerAndObjProducer();
	int32_t CollideRobotAndObjProducer();
	void ApplyForce(CFixVector force);
	void Bump(CFixVector force, fix damage);
	int Index() const
	{
		return 0;
	}
	CFixVector &Velocity()
	{
		return mType.physInfo.velocity;
	}
	void CreateSound(int) {}
	void ApplyDamageToPlayer(CObject *, fix value)
	{
		damage += value;
	}
	void ApplyDamageToRobot(fix value, int)
	{
		damage += value;
	}
	bool boss = false;
	bool IsBoss() const
	{
		return boss;
	}
};
std::vector<CObject> oracle_objects;
#define FORALL_ROBOT_OBJS(object)                                                                                    \
	for (std::size_t index = 0; index < oracle_objects.size() && ((object = &oracle_objects[index]), true); ++index) \
		if (object->info.nType == 2)
struct {
	struct {
		fix intensity = 0;
	} lightInfo;
} light_type;
struct OracleLight {
	decltype(light_type) cType;
	void SetLife(fix) {}
} light;
int CreateLight(int, int, const CFixVector &)
{
	return 0;
}
#define OBJECT(index) (&light)
int random_token = 0, random_calls = 0;
std::vector<int> random_transcript;
std::size_t random_cursor = 0;
bool contact_transcript = false;
int Rand(int maximum)
{
	if (random_cursor >= random_transcript.size()) throw std::runtime_error("Contact reference exhausted RNG transcript");
	return random_transcript[random_cursor++] % maximum;
}
int RandShort()
{
	if (contact_transcript) return Rand(32768);
	++random_calls;
	return random_token;
}
int SRandShort()
{
	return 16383 - RandShort();
}
struct tObjectProducerInfo {
	int32_t objFlags[3]{};
	int32_t xHitPoints = 0, xInterval = 0;
	int16_t nSegment = -1, nProducer = -1;
	bool bAssigned = false;
};
struct CSegment {
	int m_function = 0, m_nObjProducer = -1, m_value = -1;
	std::array<int, 8> m_vertices{};
	int passable = 0;
	std::array<CFixVector, 6> centers{};
	CFixVector m_vCenter{};
	int index = 0;
	int Index() const
	{
		return index;
	}
	bool CreateObjectProducer(int32_t, int32_t);
	bool CreateProducer(int32_t);
	bool CreateRobotMaker(int32_t);
	bool CreateEquipmentMaker(int32_t);
	bool CreateGenerator(int32_t);
	int IsPassable(int side, CObject *) const
	{
		return passable & (1 << side) ? WID_PASSABLE_FLAG : 0;
	}
	CFixVector SideCenter(int side) const
	{
		return centers[side];
	}
	int Function() const
	{
		return m_function;
	}
};
std::vector<CSegment> segments;
#define SEGMENT(index) (&segments.at(index))
struct tProducerInfo {
	int nSegment = -1, nType = 0;
	int8_t bFlag = 1;
	int8_t bEnabled = 0, nLives = 0;
	fix xCapacity = 0, xTimer = 0, xDisableTime = 0, xMaxCapacity = 0;
	CFixVector vCenter{};
};
struct {
	struct {
		int nSegments = 0;
		std::array<CFixVector, 8> vertices{};
	} segData;
	struct {
		tObjectProducerInfo robotMakers[500]{}, equipmentMakers[500]{};
		tProducerInfo producers[MAX_FUEL_CENTERS]{};
		int origProducerTypes[MAX_FUEL_CENTERS]{};
		int nRobotMakers = 0, nEquipmentMakers = 0, nProducers = 0, nRepairCenters = 0;
	} producerData;
	struct {
		std::uint32_t nVertigoBotFlags = 0;
		CObject *pConsole = nullptr;
		struct Boost {
			int bBoosted = 0;
		} speedBoost[1];
	} objData;
	struct {
		int nTypes[2]{};
		tRobotInfo info[2][100]{};
	} botData;
} gameData;
struct {
	struct {
		int count = 0;
	} botGen, equipGen;
} gameFileInfo;
void PrintLog(int, const char *, ...)
{
	throw std::runtime_error("Unexpected producer diagnostic");
}
struct {
	bool active = false;
	bool Active() const
	{
		return active;
	}
} automap;
tRobotInfo *ROBOTINFO(int id)
{
	return &gameData.botData.info[0][id];
}
void CreateExplosion(int, const CFixVector &, fix, int) {}
struct CFile {
	const xl_compat::Bytes &bytes;
	std::size_t position;
	int ReadInt()
	{
		std::uint32_t result = 0;
		for (int i = 0; i < 4; ++i) result |= std::uint32_t(bytes.at(position++)) << (i * 8);
		return static_cast<std::int32_t>(result);
	}
	int ReadFix()
	{
		return ReadInt();
	}
	int16_t ReadShort()
	{
		const unsigned low = bytes.at(position++);
		return static_cast<std::int16_t>(low | (unsigned(bytes.at(position++)) << 8));
	}
};
// The extracted functions retain unused parameters from their original APIs
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "xl_reference_generators.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void check(bool value, const char *why)
{
	if (!value) throw std::runtime_error(why);
}
void initialization_controls()
{
	int cases = 0;
	for (int authored : { -32768, -1, 0, 127, 128, 499, 500, 32767 })
		for (int value : { -32768, -1, 32767 }) {
			xl_compat::Inventory source;
			source.level_version = 27;
			source.game_version = 40;
			for (int function : { 3, 1, 11, 4, 0, 2, 11, 3, 4, 9 }) {
				xl_compat::Segment segment{};
				segment.function = function;
				segment.producer = authored;
				segment.value = value;
				source.segments.push_back(segment);
			}
			gameData = {};
			gameData.segData.nSegments = static_cast<int>(source.segments.size());
			gameFileInfo.botGen.count = 2;
			gameFileInfo.equipGen.count = 2;
			segments.clear();
			for (std::size_t i = 0; i < source.segments.size(); ++i) {
				const auto &segment = source.segments[i];
				segments.push_back({ segment.function, segment.producer, segment.value });
				segments.back().index = static_cast<int>(i);
			}
			for (auto &segment : segments) segment.CreateGenerator(segment.m_function);
			const auto production = xl_compat::producer_layout(source);
			check(production.robots.size() == 2 && production.equipment.size() == 2 && production.stations.size() == std::size_t(gameData.producerData.nProducers), "Mixed producer initialization counts differ");
			for (std::size_t i = 0; i < segments.size(); ++i) check(xl_compat::producer_function(source.segments[i]) == segments[i].m_function, "Wide fuel/repair producer normalization differs");
			for (std::size_t i = 0; i < production.stations.size(); ++i) check(production.stations[i] == gameData.producerData.producers[i].nSegment && segments[production.stations[i]].m_value == int(i), "Source station order includes a reactor or preserves a stale value");
			for (int function : { 4, 11 }) {
				const auto &owners = function == 4 ? production.robots : production.equipment;
				for (int i = 0; i < 2; ++i) {
					tObjectProducerInfo row;
					row.objFlags[0] = 1 << (i + function);
					row.nSegment = -1;
					row.nProducer = 32767;
					check(AssignProducer(row, i, function, function == 4 ? 1 : 2) == i, "Initialized source producer assignment failed");
					const auto &actual = function == 4 ? gameData.producerData.robotMakers[i] : gameData.producerData.equipmentMakers[i];
					check(actual.nSegment == owners[i] && actual.nProducer == segments[owners[i]].m_value && actual.objFlags[0] == row.objFlags[0], "Initialized producer row identity differs");
				}
			}
			++cases;
		}
	std::cout << cases << " mixed producer initialization cases: wide fields, repair/fuel disable boundary, reactor exclusion and both maker kinds matched\n";
}
void compare_runtime(const std::filesystem::path &root)
{
	{
		std::ifstream contacts(root / "generator-contact.txt");
		int player, map, movement, mass, size, passable, vx, vy, vz, damage, draws, cases = 0;
		CFixVector position;
		while (contacts >> player >> map >> movement >> mass >> size >> position.v.coord.x >> position.v.coord.y >> position.v.coord.z >> passable) {
			segments = { {} };
			segments[0].passable = passable;
			for (auto &center : segments[0].centers) contacts >> center.v.coord.x >> center.v.coord.y >> center.v.coord.z;
			contacts >> vx >> vy >> vz >> damage >> draws;
			random_transcript.clear();
			for (int i = 0; i < draws; ++i) {
				int token;
				contacts >> token;
				random_transcript.push_back(token);
			}
			check(bool(contacts), "truncated contact sample");
			random_cursor = 0;
			contact_transcript = true;
			CObject actor;
			actor.info.nType = player ? OBJ_PLAYER : OBJ_ROBOT;
			actor.info.movementType = movement;
			actor.info.xSize = size;
			actor.info.position.vPos = position;
			actor.mType.physInfo.mass = mass;
			actor.Velocity() = { { { 65536, -131072, 65536 / 7 } } };
			gameData.objData.pConsole = player ? &actor : nullptr;
			automap.active = map != 0;
			if (player) actor.CollidePlayerAndObjProducer();
			else actor.CollideRobotAndObjProducer();
			const auto &velocity = actor.Velocity().v.coord;
			check(velocity.x == vx && velocity.y == vy && velocity.z == vz && actor.damage == damage, "reference generator knockback/damage differs");
			check(random_cursor == random_transcript.size(), "reference contact RNG consumption differs");
			++cases;
		}
		check(cases == 96, "contact cases omitted");
		contact_transcript = false;
		std::cout << "Pinned generator contact: " << cases << " knockback/damage/RNG cases matched\n";
	}
	std::ifstream masks(root / "generator-mask.txt");
	std::uint32_t mask;
	check(bool(masks >> mask), "missing generator mask samples");
	CObject sample;
	while (masks >> sample.info.nType >> sample.info.nId >> sample.boss) oracle_objects.push_back(sample);
	SetVertigoRobotFlags();
	check(gameData.objData.nVertigoBotFlags == mask, "source implicit robot mask differs");
	std::ifstream selections(root / "generator-selection.txt");
	check(bool(selections), "missing selection samples");
	std::uint32_t flags0, flags1, seed, after_one, after;
	int selected, selection_cases = 0;
	while (selections >> flags0 >> flags1 >> mask >> seed >> random_token >> after_one >> selected >> after) {
		auto &producer = gameData.producerData.robotMakers[0];
		producer.objFlags[0] = static_cast<std::int32_t>(flags0);
		producer.objFlags[1] = static_cast<std::int32_t>(flags1);
		producer.objFlags[2] = 0;
		gameData.objData.nVertigoBotFlags = mask;
		random_calls = 0;
		check(ReferenceSelectionGate(0) == selected, "reference robot choice differs");
		check(random_calls <= 1 && after == (random_calls ? after_one : seed), "selection RNG consumption differs");
		++selection_cases;
	}
	check(selection_cases == 42, "selection cases omitted");
	std::ifstream distances(root / "generator-distance.txt");
	CFixVector point;
	fix distance;
	int distance_cases = 0;
	while (distances >> point.v.coord.x >> point.v.coord.y >> point.v.coord.z >> distance) {
		check(point.Mag() == distance, "reference generator distance differs");
		++distance_cases;
	}
	check(distance_cases == 6, "distance cases omitted");
	std::ifstream activations(root / "generator-activation.txt");
	int difficulty, lives, enabled, result_lives, result_enabled, capacity, timer, disable, activation_cases = 0;
	while (activations >> difficulty >> lives >> enabled >> result_lives >> result_enabled >> capacity >> timer >> disable) {
		gameData = {};
		gameStates.app.nDifficultyLevel = difficulty;
		gameData.producerData.nRobotMakers = 1;
		segments = { { SEGMENT_FUNC_ROBOTMAKER, 0, 0 } };
		gameData.producerData.robotMakers[0].nProducer = 0;
		auto &center = gameData.producerData.producers[0];
		center.nLives = static_cast<std::int8_t>(lives);
		center.bEnabled = static_cast<std::int8_t>(enabled);
		StartObjectProducer(0);
		check(center.nLives == result_lives && center.bEnabled == result_enabled && center.xCapacity == capacity && center.xTimer == timer && center.xDisableTime == disable, "reference difficulty/lives activation differs");
		++activation_cases;
	}
	check(activation_cases == 30, "activation cases omitted");
	std::ifstream shields(root / "generator-shields.txt");
	int kind, level, expected_shields, shield_cases = 0;
	while (shields >> kind >> level >> difficulty >> expected_shields) {
		gameStates.app.nDifficultyLevel = difficulty;
		missionManager.nCurrentLevel = level;
		gameData.botData.info[0][0] = { I2X(100), kind == 1, kind == 2, 0 };
		CObject object;
		object.info.nType = OBJ_ROBOT;
		check(RobotDefaultShield(&object) == expected_shields, "reference generated robot health differs");
		++shield_cases;
	}
	check(shield_cases == 45, "generated health cases omitted");
	std::cout << "Pinned generator runtime: mask, " << selection_cases << " selection/RNG, " << distance_cases << " distances, " << activation_cases << " activation and " << shield_cases << " actual generated-health cases matched\n";
}
int main(int argc, char **argv)
{
	try {
		initialization_controls();
		if (argc == 3 && std::string(argv[1]) == "--runtime") {
			compare_runtime(argv[2]);
			return 0;
		}
		check(argc == 2, "requires generated fixture directory");
		int cases = 0;
		for (int version : { 15, 17, 23 }) {
			std::ifstream input(std::filesystem::path(argv[1]) / ("generators-" + std::to_string(version) + ".rl2"), std::ios::binary);
			xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			const auto original = xl_compat::inspect_level(bytes);
			const auto properties = original.mine_offset + 5 + 8 * 12 + 5 * 176;
			std::array<int, 3> order{ 0, 1, 2 };
			do {
				for (int i = 0; i < 3; ++i) bytes.at(properties + (i ? i + 1 : 0) * (version == 23 ? 13 : 8) + 1) = static_cast<unsigned char>(order[i]);
				const auto source = xl_compat::inspect_level(bytes);
				gameData = {};
				gameData.segData.nSegments = static_cast<int>(source.segments.size());
				segments.clear();
				for (std::size_t i = 0; i < source.segments.size(); ++i) {
					const auto &segment = source.segments[i];
					segments.push_back({ segment.function, segment.producer, segment.value });
					segments.back().index = static_cast<int>(i);
				}
				gameFileInfo.botGen.count = static_cast<int>(source.matcens.size());
				gameFileInfo.equipGen.count = static_cast<int>(source.equipment.size());
				// LoadExtSegmentsCompiled calls these in geometry order before reading
				// the producer rows. Assignment-only tests previously omitted this stage
				for (auto &segment : segments) segment.CreateGenerator(segment.m_function);
				const auto production = xl_compat::producer_layout(source);
				check(production.robots.size() == std::size_t(gameData.producerData.nRobotMakers) && production.equipment.size() == std::size_t(gameData.producerData.nEquipmentMakers) && production.stations.size() == std::size_t(gameData.producerData.nProducers), "Initialized producer counts differ");
				for (std::size_t i = 0; i < segments.size(); ++i) {
					check(xl_compat::producer_function(source.segments[i]) == segments[i].m_function, "Initialized producer function differs");
					if (segments[i].m_function == 4) check(production.robot_index[i] == segments[i].m_nObjProducer, "Initialized robot row selection differs");
				}
				const auto section = std::find_if(source.sections.begin(), source.sections.end(), [](const auto &entry) { return entry.name == "matcens"; });
				CFile file{ bytes, static_cast<std::size_t>(section->offset) };
				for (int i = 0; i < section->count; ++i) {
					tObjectProducerInfo row;
					ReadObjectProducerInfo(&row, file, false);
					check(AssignProducer(row, i, SEGMENT_FUNC_ROBOTMAKER, 1) == i, "reference failed valid source assignment");
				}
				check(file.position == std::size_t(section->offset + section->count * 20), "reference disk extent differs");
				const auto translated = xl_compat::prepare_gameplay_probe(bytes).stock_level;
				CFile header{ translated, 12 };
				const auto game = header.ReadInt();
				header.position = game + 35 + 6 * 12;
				CFile native{ translated, static_cast<std::size_t>(header.ReadInt()) };
				for (std::size_t i = 0; i < segments.size(); ++i) {
					const auto &segment = segments[i];
					if (segment.m_function != SEGMENT_FUNC_ROBOTMAKER) continue;
					tObjectProducerInfo row;
					ReadObjectProducerInfo(&row, native, false);
					const auto &expected = gameData.producerData.robotMakers[segment.m_nObjProducer];
					check(row.objFlags[0] == expected.objFlags[0] && row.objFlags[1] == expected.objFlags[1], "translated selection differs from extracted XL assignment");
					check(row.nSegment == expected.nSegment && row.nProducer == expected.nProducer, "translated links differ from extracted XL assignment");
				}
				++cases;
			} while (std::next_permutation(order.begin(), order.end()));
		}
		std::cout << "Pinned generator initialization and assignment: " << cases << " three-layout/permutation cases matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
