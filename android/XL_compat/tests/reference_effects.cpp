// Storage and engine adapters for the unchanged pinned CObject::Read body
#include "inventory.h"
#include "ambient_sound.h"
#include "hog.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#define DBG                       0
#define OBJ_IDX(object)           0
#define OBJ_REACTOR               9
#define OBJ_EFFECT                20
#define PARTICLE_ID               0
#define LIGHTNING_ID              1
#define SOUND_ID                  2
#define WAYPOINT_ID               3
#define MT_NONE                   0
#define MT_PHYSICS                1
#define MT_SPINNING               3
#define CT_NONE                   0
#define CT_AI                     1
#define CT_EXPLOSION              2
#define CT_FLYING                 4
#define CT_SLEW                   5
#define CT_FLYTHROUGH             6
#define CT_WEAPON                 9
#define CT_REPAIRCEN              10
#define CT_MORPH                  11
#define CT_DEBRIS                 12
#define CT_POWERUP                13
#define CT_LIGHT                  14
#define CT_CNTRLCEN               16
#define CT_WAYPOINT               17
#define RT_NONE                   0
#define RT_POLYOBJ                1
#define RT_FIREBALL               2
#define RT_LASER                  3
#define RT_HOSTAGE                4
#define RT_POWERUP                5
#define RT_MORPH                  6
#define RT_WEAPON_VCLIP           7
#define RT_THRUSTER               8
#define RT_PARTICLES              11
#define RT_LIGHTNING              12
#define RT_SOUND                  13
#define MAX_AI_FLAGS              11
#define MAX_SUBMODELS             10
#define MAX_ANIMATIONS_D2         110
#define POW_VULCAN                13
#define POW_GAUSS                 28
#define POW_OMEGA                 31
#define POW_VULCAN_AMMO           22
#define VULCAN_WEAPON_AMMO_AMOUNT 196
#define VULCAN_CLIP_CAPACITY      98
#define MAX_OMEGA_CHARGE          65536
#define I2X(value)                ((value) * 65536)
using Vector = std::array<std::int32_t, 3>;
using Matrix = std::array<std::int32_t, 9>;
using Angles = std::array<std::int16_t, 3>;
class CFile
{
	const xl_compat::Bytes &bytes;
	std::size_t cursor, end;

  public:
	std::vector<int> integers, filename;
	CFile(const xl_compat::Bytes &source, std::size_t start, std::size_t limit) : bytes(source), cursor(start), end(limit)
	{
		if (start > limit || limit > bytes.size()) throw std::runtime_error("Invalid reference object extent");
	}
	int ReadUByte()
	{
		if (cursor >= end) throw std::runtime_error("Truncated reference object");
		return bytes[cursor++];
	}
	int ReadByte()
	{
		const int value = ReadUByte();
		return value < 128 ? value : value - 256;
	}
	std::int16_t ReadShort()
	{
		const int low = ReadUByte(), high = ReadUByte();
		const int value = low | (high << 8);
		return static_cast<std::int16_t>(value < 32768 ? value : value - 65536);
	}
	std::int32_t ReadInt()
	{
		std::uint32_t value = 0;
		for (int i = 0; i < 4; ++i) value |= std::uint32_t(ReadUByte()) << (i * 8);
		const auto result = static_cast<std::int32_t>(value <= INT32_MAX ? std::int64_t(value) : std::int64_t(value) - INT64_C(4294967296));
		integers.push_back(result);
		return result;
	}
	std::int32_t ReadFix()
	{
		return ReadInt();
	}
	std::int16_t ReadFixAng()
	{
		return ReadShort();
	}
	void ReadVector(Vector &value)
	{
		for (auto &field : value) field = ReadInt();
	}
	void ReadMatrix(Matrix &value)
	{
		for (auto &field : value) field = ReadInt();
	}
	void ReadAngVec(Angles &value)
	{
		for (auto &field : value) field = ReadShort();
	}
	void Read(char *destination, int size, int count)
	{
		filename.clear();
		for (int i = 0; i < size * count; ++i) {
			const int byte = ReadUByte();
			filename.push_back(byte);
			destination[i] = static_cast<char>(byte);
		}
	}
	std::size_t Tell() const
	{
		return cursor;
	}
};
struct Color {
	std::array<std::uint8_t, 4> bytes{};
	std::uint8_t &Red()
	{
		return bytes[0];
	}
	std::uint8_t &Green()
	{
		return bytes[1];
	}
	std::uint8_t &Blue()
	{
		return bytes[2];
	}
	std::uint8_t &Alpha()
	{
		return bytes[3];
	}
};
struct {
	struct {
		bool bD1Mission = false;
	} app;
} gameStates;
struct {
	int fileinfoVersion = 0;
} gameTopFileInfo;
struct {
	struct {
		int nLevelVersion = 0;
	} segData;
	struct {
		struct {
			struct {
				int nClipIndex = 0;
			} info[256];
		} pwrUp;
	} objData;
	struct {
		struct {
			int xFrameTime = 0;
		} vClipP[256];
	} effectData;
} gameData;
int warnings = 0;
void PrintLog(int, const char *, ...)
{
	++warnings;
}
char *reference_strlwr(char *text)
{
	for (char *p = text; *p; ++p)
		if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
	return text;
}
struct CObject {
	struct {
		std::uint8_t nType = 0, nId = 0, controlType = 0, movementType = 0, renderType = 0, nFlags = 0;
		int nSegment = 0, nAttachedObj = 0;
		struct {
			Vector vPos{};
			Matrix mOrient{};
		} position;
		Vector vLastPos{};
		struct {
			std::int8_t nType = 0, nId = 0, nCount = 0;
		} contains;
	} info;
	bool m_bMultiplayer = false;
	int m_nAttackRobots = 0, size = 0, shield = 0;
	struct {
		struct {
			int behavior = 0, flags[MAX_AI_FLAGS]{}, nHideSegment = 0, nHideIndex = 0, nPathLength = 0, nCurPathIndex = 0;
		} aiInfo;
		struct {
			int nSpawnTime = 0, nDeleteTime = 0, nDestroyedObj = 0;
			struct {
				int nNext = 0, nPrev = 0, nParent = 0;
			} attached;
		} explInfo;
		struct {
			struct {
				int nType = 0, nObject = 0, nSignature = 0;
			} parent;
		} laserInfo;
		struct {
			int intensity = 0;
		} lightInfo;
		struct {
			int nCount = 0;
		} powerupInfo;
		struct {
			int nId[2]{}, nSuccessor[2]{}, nSpeed = 0;
		} wayPointInfo;
	} cType;
	struct {
		struct {
			Vector velocity{}, thrust{}, rotVel{}, rotThrust{};
			int mass = 0, drag = 0, brakes = 0, turnRoll = 0, flags = 0;
		} physInfo;
		Vector spinRate{};
	} mType;
	struct {
		struct {
			int nModel = 0;
			Angles animAngles[MAX_SUBMODELS]{};
			int nSubObjFlags = 0, nTexOverride = 0, nAltTextures = 0;
		} polyObjInfo;
		struct {
			int nClipIndex = 0, xFrameTime = 0, nCurFrame = 0;
		} animationInfo;
		struct {
			int nLife = 0, nSize[2]{}, nParts = 0, nSpeed = 0, nDrift = 0, nBrightness = 0;
			Color color;
			char nSide = 0, nType = 0, bEnabled = 0;
		} particleInfo;
		struct {
			int nLife = 0, nDelay = 0, nLength = 0, nAmplitude = 0, nOffset = 0, nWayPoint = 0, nBolts = 0, nId = 0, nTarget = 0, nNodes = 0, nChildren = 0, nFrames = 0;
			char nWidth = 0, nAngle = 0, nStyle = 0, nSmoothe = 0, bClamp = 0, bGlow = 0, bBlur = 0, bSound = 0, bRandom = 0, bInPlane = 0, bDirection = 0, bEnabled = 0;
			Color color;
		} lightningInfo;
		struct {
			char szFilename[40]{};
			int nVolume = 0;
			char bEnabled = 0;
		} soundInfo;
	} rType;
	void SetSize(int value)
	{
		size = value;
	}
	void SetShield(int value)
	{
		shield = value;
	}
	void ResetDamage() {}
	void SetTarget(void *) {}
	void Read(CFile &file);
	void FixEffectRenderType();
};
#define strlwr reference_strlwr
#include "xl_reference_effects.h"
#undef strlwr

static void volume_controls(const xl_compat::Bytes &bytes, const xl_compat::Object &object, int version)
{
	auto source = bytes;
	const auto offset = object.source_end - (version >= 19 ? 5 : 4);
	std::vector<int> values{ -327680, -327679, -327678, -65537, -65536, -11, -10, -9, -2, -1, 0,
		                     1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 100, 65535, 65536, 65537, 327678, 327679 };
	std::uint32_t seed = 1;
	for (int i = 0; i < 1024; ++i) {
		seed = seed * 1664525u + 1013904223u;
		values.push_back(int(seed % 655359u) - 327679);
	}
	for (int value : values) {
		for (unsigned byte = 0; byte < 4; ++byte) source[offset + byte] = std::uint8_t(std::uint32_t(value) >> (8 * byte));
		CFile file(source, object.source_offset, object.source_end);
		CObject reference;
		reference.Read(file);
		if (file.Tell() != object.source_end || xl_compat::ambient_sound_volume(value) != reference.rType.soundInfo.nVolume)
			throw std::runtime_error("Actual source ambient volume differs at serialized value " + std::to_string(value));
	}
	for (int value : { INT32_MIN, -327681, 327680, INT32_MAX }) {
		bool rejected = false;
		try {
			(void) xl_compat::ambient_sound_volume(value);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Out-of-range ambient volume reached an undefined integer conversion");
	}
	std::cout << "PASS: " << values.size() << " actual source ambient volume conversions and four overflow rejections\n";
}

static void compare_level(const xl_compat::Bytes &bytes, int &objects, int &effects)
{
	const auto level = xl_compat::inspect_level(bytes);
	gameData.segData.nLevelVersion = level.level_version;
	gameTopFileInfo.fileinfoVersion = level.game_version;
	for (const auto &native : level.objects) {
		CFile file(bytes, native.source_offset, native.source_end);
		CObject reference;
		reference.Read(file);
		if (file.Tell() != native.source_end || reference.info.nType != native.type || reference.info.nId != native.reader_id || reference.info.renderType != native.reader_render || reference.info.controlType != native.control || reference.info.movementType != native.movement || reference.info.nSegment != native.segment || reference.info.position.vPos != native.position || reference.info.position.mOrient != native.orientation)
			throw std::runtime_error("Reference object header, normalized identity or consumed end differs at object " + std::to_string(objects));
		std::vector<int> fields;
		const auto byte = [](char value) { return static_cast<unsigned char>(value); };
		if (native.render == RT_PARTICLES) {
			const auto &p = reference.rType.particleInfo;
			fields = { p.nLife, p.nSize[0], p.nParts, p.nSpeed, p.nDrift, p.nBrightness };
			for (auto value : p.color.bytes) fields.push_back(value);
			fields.insert(fields.end(), { byte(p.nSide), byte(p.nType), byte(p.bEnabled) });
		} else if (native.render == RT_LIGHTNING) {
			const auto &p = reference.rType.lightningInfo;
			fields = { p.nLife, p.nDelay, p.nLength, p.nAmplitude, p.nOffset, p.nWayPoint, p.nBolts, p.nId, p.nTarget, p.nNodes, p.nChildren, p.nFrames,
				       byte(p.nWidth), byte(p.nAngle), byte(p.nStyle), byte(p.nSmoothe), byte(p.bClamp), byte(p.bGlow), byte(p.bSound), byte(p.bRandom), byte(p.bInPlane) };
			for (auto value : p.color.bytes) fields.push_back(value);
			fields.push_back(byte(p.bEnabled));
		} else if (native.render == RT_SOUND) {
			static bool volumes_checked = false;
			if (!volumes_checked) {
				volume_controls(bytes, native, level.level_version);
				volumes_checked = true;
			}
			const auto loaded = xl_compat::decode_ambient_sound(native);
			if (loaded.volume != reference.rType.soundInfo.nVolume || loaded.enabled != bool(reference.rType.soundInfo.bEnabled))
				throw std::runtime_error("Reference sound runtime volume or enabled state differs");
			fields = file.filename;
			fields.push_back(file.integers.back());
			fields.push_back(byte(reference.rType.soundInfo.bEnabled));
			std::string name;
			bool ascii = true;
			for (int i = 0; i < 39 && native.render_data[i]; ++i) {
				const int c = native.render_data[i];
				if (c < 32 || c > 126) {
					ascii = false;
					break;
				}
				name += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
			}
			if (ascii && name != reference.rType.soundInfo.szFilename) throw std::runtime_error("Reference sound filename normalization differs");
		}
		if ((native.render == RT_PARTICLES || native.render == RT_LIGHTNING || native.render == RT_SOUND) && fields != native.render_data)
			throw std::runtime_error("Reference effect fields/defaults differ at object " + std::to_string(objects));
		if (native.control == CT_WAYPOINT && native.control_data != std::vector<int>{ reference.cType.wayPointInfo.nId[1], reference.cType.wayPointInfo.nSuccessor[0], reference.cType.wayPointInfo.nSpeed }) throw std::runtime_error("Reference waypoint fields differ");
		++objects;
		if (native.render >= 8) ++effects;
	}
}

void effect_target_source_controls();
void particle_source_controls();
void particle_geometry_source_controls();
void particle_fire_source_controls();
void particle_world_source_controls();
void compare_particle_world_receipt(const char *);
void particle_original_geometry_controls(const xl_compat::Inventory &);
void static_explosion_source_controls();
void static_explosion_render_source_controls();
void static_explosion_runtime_controls(const char *);
void ambient_activation_runtime_controls(const char *);
int main(int argc, char **argv)
{
	try {
		if (argc == 3 && std::string(argv[1]) == "--particle-world") {
			compare_particle_world_receipt(argv[2]);
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--static-runtime") {
			static_explosion_runtime_controls(argv[2]);
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--ambient-runtime") {
			ambient_activation_runtime_controls(argv[2]);
			return 0;
		}
		if (argc != 2) throw std::runtime_error("Expected effect fixture directory or original HOG");
		effect_target_source_controls();
		particle_source_controls();
		particle_geometry_source_controls();
		particle_fire_source_controls();
		particle_world_source_controls();
		static_explosion_source_controls();
		static_explosion_render_source_controls();
		int objects = 0, effects = 0, levels = 0;
		const auto read = [](const std::filesystem::path &path) {
			std::ifstream input(path, std::ios::binary);
			if (!input) throw std::runtime_error("Cannot read effect fixture");
			return xl_compat::Bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		};
		if (std::filesystem::is_directory(argv[1])) {
			for (const auto &entry : std::filesystem::directory_iterator(argv[1])) {
				if (entry.path().extension() != ".rl2") continue;
				compare_level(read(entry.path()), objects, effects);
				++levels;
			}
			if (levels != 125) throw std::runtime_error("Missing effect reference profiles");
		} else {
			std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
			if (!input || input.tellg() < 3) throw std::runtime_error("Cannot read original effect HOG");
			const auto length = std::uint64_t(input.tellg());
			const auto read_at = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
				if (offset > length || count > length - offset) throw std::runtime_error("Original effect read outside HOG");
				input.clear();
				input.seekg(static_cast<std::streamoff>(offset));
				input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
				if (!input) throw std::runtime_error("Truncated original effect HOG");
			};
			for (const auto &member : xl_compat::index_hog(length, read_at)) {
				const auto name = xl_compat::hog_key(member.name);
				if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
				if (member.size > 64 * 1024 * 1024) throw std::runtime_error("Original effect level exceeds reader bound");
				xl_compat::Bytes bytes(static_cast<std::size_t>(member.size));
				read_at(member.offset, bytes.data(), bytes.size());
				try {
					xl_compat::inspect_level(bytes);
				} catch (const std::runtime_error &e) {
					std::cout << member.name << ": not compared: " << e.what() << '\n';
					continue;
				}
				compare_level(bytes, objects, effects);
				particle_original_geometry_controls(xl_compat::inspect_level(bytes));
				++levels;
			}
			if (!levels) throw std::runtime_error("No completely decoded original levels for comparison");
		}
		std::cout << "Pinned XL object reader: " << levels << " levels, " << objects << " object ends/identities and " << effects << " effect payloads matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
