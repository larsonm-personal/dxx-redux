// Storage/allocator adapters around the actual pinned equipment functions
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define ENTER(...)
#define RETVAL(value)        return (value);
#define RETURN               return;
#define TRACE                0
#define DBG                  0
#define IsMultiGame          false
#define I2X(value)           ((value) * 65536)
#define EQUIP_GEN_TIME       (I2X(3) * (gameStates.app.nDifficultyLevel + 1))
#define MAX_POWERUP_TYPES    50
#define SUSP_POWERUPS        4
#define OBJ_PLAYER           4
#define OBJ_POWERUP          7
#define CT_POWERUP           13
#define MT_PHYSICS           1
#define RT_POWERUP           5
#define POW_VULCAN_AMMO      22
#define VULCAN_CLIP_CAPACITY 98
#define ANIM_MORPHING_ROBOT  10
#define IMMORTAL_TIME        0x3fffffff
#define EF_ONE_SHOT          2
using fix = std::int32_t;
struct CFixVector {
	std::array<fix, 3> xyz{};
};
struct CFixMatrix {
	std::array<fix, 9> values{};
	static const CFixMatrix IDENTITY;
};
const CFixMatrix CFixMatrix::IDENTITY{ { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 } };
struct CObject {
	struct {
		int nType = 0, nId = 0, nNextInSeg = -1, nCreator = -1, controlType = 0, movementType = 0, renderType = 0;
		fix xSize = 0, life = IMMORTAL_TIME;
		struct {
			CFixVector vPos;
			CFixMatrix mOrient;
		} position;
	} info;
	struct {
		struct {
			int nCount = 1;
		} powerupInfo;
	} cType;
	struct {
		struct {
			int nClipIndex = -1;
			fix xFrameTime = 0;
			int nCurFrame = 0;
		} animationInfo;
	} rType;
	int index = 0;
	int Index() const
	{
		return index;
	}
	void SetLife(fix value)
	{
		info.life = value;
	}
	static int IsEquipment(int)
	{
		throw std::runtime_error("Unqualified equipment cheat branch");
	}
};
std::vector<CObject> objects;
#define OBJECT(index) ((index) < 0 ? nullptr : &objects.at(index))
struct CSegment {
	int m_nObjProducer = 0, m_objects = -1, m_owner = -1, index = 0;
	struct Side {
		int m_nOvlTex = 0;
	} m_sides[6];
	CFixVector center;
	CFixMatrix orient;
	CFixVector Center() const
	{
		return center;
	}
	int Index() const
	{
		return index;
	}
};
std::vector<CSegment> segments(1);
#define SEGMENT(index) (&segments.at(index))
struct tProducerInfo {
	int nSegment = 0, bEnabled = 0, bFlag = 0;
	fix xTimer = 0;
};
struct CTrigger {
	int m_nLinks = 0;
	int m_segments[10]{}, m_sides[10]{};
	struct {
		int nObject = -1;
	} m_info;
	int32_t HasTarget(int16_t segment, int16_t side);
};
struct CWall {
	int nTrigger = -1, nSegment = 0, nSide = 0;
};
struct tEffectInfo {
	int flags = 0;
	struct {
		int nTexture = -1;
	} destroyed;
};
std::vector<CTrigger> geometry_triggers, object_triggers;
std::vector<CWall> walls;
#define GEOTRIGGER(index) (&geometry_triggers.at(index))
#define OBJTRIGGER(index) (&object_triggers.at(index))
#define WALL(index)       (&walls.at(index))
#define FORALL_OBJS(object)                                                                \
	for (std::size_t it = 0; it < objects.size() && ((object = &objects[it]), true); ++it) \
		if (object->info.nType != 255)
int32_t FindTriggerTarget(int16_t segment, int16_t side, int32_t start = 0);
struct {
	struct {
		int nDifficultyLevel = 0, bGameSuspended = 0;
	} app;
	struct {
		bool bMineMineCheat = false;
	} gameplay;
} gameStates;
struct {
	struct {
		fix xFrame = 0;
	} timeData;
	struct {
		struct {
			std::int32_t objFlags[3]{};
			int nProducer = 0;
		} equipmentMakers[128];
		int nEquipmentMakers = 0;
		tProducerInfo producers[256];
	} producerData;
	struct {
		struct Animation {
			fix xTotalTime = 0, xFrameTime = 0;
			int nSound = -1;
		};
		std::array<std::vector<Animation>, 1> animations;
		tEffectInfo effects[1];
		tEffectInfo *pEffect = effects;
	} effectData;
	struct {
		struct {
			struct {
				int nEffectClip = -1, destroyed = -1;
			} pTexMapInfo[2];
		} tex;
	} pigData;
	struct {
		int nWalls = 0;
	} wallData;
	struct {
		int m_nTriggers[2]{};
		struct Ref {
			int nFirst = 0, nCount = 0;
		};
		std::vector<Ref> objTriggerRefs;
	} trigData;
	struct {
		struct {
			struct {
				fix size = 0;
				int nClipIndex = 0;
			} info[50];
		} pwrUp;
	} objData;
	struct {
		int maxPowerupsAllowed[50]{}, powerupsInMine[50]{};
	} multiplayer;
	struct {
		struct {
			int nObjNums[1]{}, nCount = 0;
		} create;
	} multigame;
} gameData;
struct {
	void CreateSegmentSound(int, int, int, CFixVector, int, fix) {}
} audio;
std::vector<int> tokens;
std::size_t cursor = 0;
int RandShort()
{
	if (cursor >= tokens.size()) throw std::runtime_error("Equipment reference exhausted RNG transcript");
	return tokens[cursor++];
}
bool effect_available = true, item_available = true;
int effect_count = 0, item_index = -1;
CObject *CreateExplosion(int, CFixVector, fix, int)
{
	if (!effect_available) return nullptr;
	++effect_count;
	objects.emplace_back();
	return &objects.back();
}
void ExtractOrientFromSegment(CFixMatrix *orient, CSegment *segment)
{
	*orient = segment->orient;
}
int CreateObject(int type, int id, int creator, int, CFixVector position, CFixMatrix orient, fix size, int control, int movement, int render)
{
	if (!item_available) return -1;
	CObject object;
	object.info.nType = type;
	object.info.nId = id;
	object.info.nCreator = creator;
	object.info.position = { position, orient };
	object.info.xSize = size;
	object.info.controlType = control;
	object.info.movementType = movement;
	object.info.renderType = render;
	item_index = static_cast<int>(objects.size());
	objects.push_back(object);
	return item_index;
}
bool TooManyPowerups(int)
{
	throw std::runtime_error("Unqualified powerup limit branch");
}
int PowerupsOnShips(int)
{
	throw std::runtime_error("Unqualified powerup limit branch");
}
int PowerupClass(int)
{
	throw std::runtime_error("Unqualified multiplayer branch");
}
void AddPowerupInMine(int)
{
	throw std::runtime_error("Unqualified multiplayer branch");
}
void PrintLog(int, const char *, ...)
{
	throw std::runtime_error("Unexpected powerup definition");
}
// Keep the hash-pinned reference's original indentation intact
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_equipment.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void initialization_controls(const char *path)
{
	std::ifstream input(path);
	int scenario, cases = 0;
	while (input >> scenario) {
		int station_count, equipment_count;
		input >> station_count >> equipment_count;
		if (station_count < equipment_count || station_count > 128 || equipment_count != 2) throw std::runtime_error("Invalid equipment initialization inputs");
		segments.assign(9000, {});
		for (int i = 0; i < station_count; ++i) input >> gameData.producerData.producers[i].nSegment;
		gameData.producerData.nEquipmentMakers = equipment_count;
		for (int i = 0; i < equipment_count; ++i) input >> gameData.producerData.equipmentMakers[i].nProducer;
		int target, wall_count;
		input >> target >> wall_count;
		walls.resize(wall_count);
		gameData.wallData.nWalls = wall_count;
		for (auto &wall : walls) {
			int overlay;
			input >> wall.nTrigger >> wall.nSegment >> wall.nSide >> overlay;
			segments.at(wall.nSegment).m_sides[wall.nSide].m_nOvlTex = overlay;
		}
		input >> gameData.pigData.tex.pTexMapInfo[1].nEffectClip >> gameData.pigData.tex.pTexMapInfo[1].destroyed >> gameData.effectData.effects[0].flags >> gameData.effectData.effects[0].destroyed.nTexture;
		geometry_triggers.assign(1, {});
		geometry_triggers[0].m_nLinks = 1;
		geometry_triggers[0].m_segments[0] = target;
		int object_count, owner, type;
		input >> object_count >> owner >> type;
		if (object_count < 0 || object_count > 1 || owner < -1 || owner > 1000) throw std::runtime_error("Invalid initialization actor");
		object_triggers.assign(object_count, geometry_triggers[0]);
		objects.assign(owner >= 0 ? owner + 1 : 1, {});
		gameData.trigData.objTriggerRefs.assign(objects.size(), {});
		for (std::size_t i = 0; i < objects.size(); ++i) {
			objects[i].info.nType = 255;
			objects[i].index = static_cast<int>(i);
		}
		if (object_count) {
			object_triggers[0].m_info.nObject = owner;
			if (owner >= 0) {
				objects[owner].info.nType = type;
				gameData.trigData.objTriggerRefs[owner].nCount = 1;
			}
		}
		gameData.trigData.m_nTriggers[0] = 1;
		gameData.trigData.m_nTriggers[1] = object_count;
		SetEquipmentMakerStates();
		for (int i = 0; i < equipment_count; ++i) {
			int expected;
			input >> expected;
			if (!input || gameData.producerData.producers[gameData.producerData.equipmentMakers[i].nProducer].bEnabled != expected)
				throw std::runtime_error("Equipment initialization mismatch in case " + std::to_string(scenario));
		}
		++cases;
	}
	if (cases != 13) throw std::runtime_error("Missing equipment initialization cases");
	std::cout << "Pinned XL equipment: 13 producer-order, destructible-wall and object-owner initialization cases matched\n";
}

int main(int argc, char **argv)
{
	try {
		if (argc != 3) throw std::runtime_error("Expected production and initialization equipment samples");
		std::ifstream input(argv[1]);
		int clips;
		input >> clips;
		if (!input || clips < 11 || clips > 256) throw std::runtime_error("Invalid equipment clip inputs");
		gameData.effectData.animations[0].resize(clips);
		for (auto &clip : gameData.effectData.animations[0]) input >> clip.xTotalTime >> clip.xFrameTime >> clip.nSound;
		for (auto &item : gameData.objData.pwrUp.info) input >> item.size >> item.nClipIndex;
		int difficulty, scenario, cases = 0;
		while (input >> difficulty >> scenario) {
			std::uint32_t first, second;
			tProducerInfo producer;
			int object_count, suspended;
			input >> first >> second >> producer.xTimer >> producer.bEnabled >> producer.bFlag >> gameData.timeData.xFrame >> suspended >> effect_available >> item_available >> object_count;
			gameStates.app = { difficulty, suspended };
			gameData.producerData.equipmentMakers[0].objFlags[0] = static_cast<std::int32_t>(first);
			gameData.producerData.equipmentMakers[0].objFlags[1] = static_cast<std::int32_t>(second);
			objects.clear();
			segments[0] = {};
			for (int i = 0; i < object_count; ++i) {
				CObject object;
				input >> object.info.nType >> object.info.nId;
				object.info.nNextInSeg = i + 1 < object_count ? i + 1 : -1;
				objects.push_back(object);
			}
			segments[0].m_objects = object_count ? 0 : -1;
			int token_count;
			input >> token_count;
			tokens.resize(token_count);
			for (auto &token : tokens) input >> token;
			fix expected_timer;
			int expected_enabled, expected_pending, expected_effects, expected_id, expected_count;
			input >> expected_timer >> expected_enabled >> expected_pending >> expected_effects >> expected_id >> expected_count;
			item_index = -1;
			effect_count = 0;
			cursor = 0;
			if (!(suspended & SUSP_POWERUPS)) EquipmentMakerHandler(&producer);
			const int id = item_index < 0 ? -1 : objects[item_index].info.nId;
			const int count = item_index < 0 ? 0 : objects[item_index].cType.powerupInfo.nCount;
			if (!input || producer.xTimer != expected_timer || producer.bEnabled != expected_enabled || producer.bFlag != expected_pending || effect_count != expected_effects || id != expected_id || count != expected_count || cursor != tokens.size())
				throw std::runtime_error("Equipment reference mismatch at difficulty " + std::to_string(difficulty) + ", scenario " + std::to_string(scenario));
			++cases;
		}
		if (cases != 210) throw std::runtime_error("Missing equipment production cases");
		std::cout << "Pinned XL equipment: " << cases << " timing/blocking/selection/allocation/suspension/RNG cases matched\n";
		initialization_controls(argv[2]);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
