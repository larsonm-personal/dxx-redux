// Storage adapters around the extracted readers. Redux supplies only the result
// being checked, never the reference's decoded state or field boundaries
#include "inventory.h"
#include "hog.h"
#include "layout_fixture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define DBG                     0
#define SEGMENT_SIDE_COUNT      6
#define SEGMENT_VERTEX_COUNT    8
#define MAX_WALL_TEXTURES       910
#define GAME_COMPATIBLE_VERSION 22
#define IS_WALL(wall)           ((wall) != 0xffff)
#define I2X(value)              ((value) * 65536)
#define X2F(value)              (float(value) / 65536.0f)
using fix = std::int32_t;
bool bNewFileFormat = true;
void check(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
void Error(const char *message)
{
	throw std::runtime_error(message);
}
int ConvertD1Texture(int, int)
{
	throw std::runtime_error("Unexpected D1 reader branch");
}
struct CFile {
	const xl_compat::Bytes &bytes;
	std::size_t position = 0;
	int ReadUByte()
	{
		return bytes.at(position++);
	}
	int ReadByte()
	{
		const int n = ReadUByte();
		return n < 128 ? n : n - 256;
	}
	int ReadUShort()
	{
		const int low = ReadUByte();
		return low | (ReadUByte() << 8);
	}
	int ReadShort()
	{
		const int n = ReadUShort();
		return n < 32768 ? n : n - 65536;
	}
	int ReadInt()
	{
		const auto low = std::uint32_t(ReadUShort());
		return static_cast<std::int32_t>(low | (std::uint32_t(ReadUShort()) << 16));
	}
	int ReadFix()
	{
		return ReadInt();
	}
	int Seek(int offset, int origin)
	{
		check(origin == SEEK_SET && offset >= 0 && std::size_t(offset) <= bytes.size(), "Reference seek outside input");
		position = static_cast<std::size_t>(offset);
		return 0;
	}
	void Read(void *out, std::size_t size, std::size_t count)
	{
		auto *target = static_cast<unsigned char *>(out);
		for (std::size_t i = 0; i < size * count; ++i) target[i] = static_cast<unsigned char>(ReadUByte());
	}
};
struct CFogData {
	struct Color {
		struct {
			struct {
				float r = 0, g = 0, b = 0, a = 0;
			} color;
		} v;
		void Set(float r, float g, float b, float a)
		{
			v.color = { r, g, b, a };
		}
	} m_color;
	void Init(int32_t);
	void Read(CFile &);
};
struct {
	struct {
		int nLevelVersion = 0, nVertices = 0;
		int nFogSegments[4]{};
		CFogData fogData[4];
		void ReadFogData(CFile &cf)
		{
			for (auto &fog : fogData) fog.Read(cf);
		}
		void InitFogData()
		{
			for (int i = 0; i < 4; ++i) fogData[i].Init(i);
		}
	} segData;
	struct {
		struct {
			std::vector<float> vertBright;
		} color;
	} renderData;
} gameData;
struct {
	struct {
		bool bD2XLevel = true;
	} app;
} gameStates;
struct CSide {
	int m_nFrame = 0, m_nShape = 0, m_nCorners = 4, m_nWall = 0xffff;
	int m_nBaseTex = 0, m_nOvlTex = 0, m_nOvlOrient = 0;
	std::array<uint8_t, 4> m_corners{};
	struct {
		fix u = 0, v = 0, l = 0;
	} m_uvls[4];
	int32_t Read(CFile &, uint16_t *, uint16_t *, bool);
	void ReadWallNum(CFile &, bool);
	void RemapVertices(uint8_t *);
};
struct CSegment {
	int m_owner = -1, m_group = -1, m_objects = -1, m_nShape = 0, m_nVertices = 8;
	uint8_t m_function = 0, m_flags = 0, m_props = 0;
	int m_nObjProducer = -1, m_value = 0;
	fix m_xDamage[2]{}, m_xAvgSegLight = 0;
	uint16_t m_vertices[8]{};
	std::array<int, 6> m_children{};
	CSide m_sides[6];
	void GetCornerIndex(int32_t, uint16_t *);
	void ReadFunction(CFile &, uint8_t);
	void ReadVerts(CFile &);
	void ReadChildren(CFile &, uint8_t);
	void ReadExtras(CFile &);
	void Read(CFile &);
	void RemapVertices();
	void Upgrade()
	{
		throw std::runtime_error("Unexpected pre-v21 extras in modern oracle");
	}
	int FogType() const
	{
		return m_props & 1 ? 1 : m_props & 2 ? 2
		                     : m_props & 32  ? 3
		                     : m_props & 64  ? 4
		                                     : 0;
	}
};
struct Section {
	int offset = -1, count = 0, size = 0;
	void Read(CFile &cf)
	{
		offset = cf.ReadInt();
		count = cf.ReadInt();
		size = cf.ReadInt();
	}
};
struct {
	int fileinfoSignature = 0, fileinfoVersion = 0, fileinfo_sizeof = 0, level = 0;
	char mineFilename[15]{};
	Section player, objects, walls, doors, triggers, links, control, botGen, lightDeltaIndices, lightDeltas, equipGen;
} gameFileInfo, gameTopFileInfo;
#include "xl_reference_layouts.h"

std::size_t comparisons = 0, level_count = 0, segment_count = 0, side_count = 0;
void equal(bool ok, const char *message)
{
	check(ok, message);
	++comparisons;
}
void compare(const xl_compat::Bytes &bytes)
{
	const auto decoded = xl_compat::inspect_level(bytes);
	CFile header{ bytes, 4 };
	gameData = {};
	gameData.segData.nLevelVersion = header.ReadInt();
	const int mine = header.ReadInt(), game = header.ReadInt();
	CFile geometry{ bytes, std::size_t(mine) };
	equal(geometry.ReadUByte() == 0, "Compiled version differs");
	const int vertices = geometry.ReadShort(), count = geometry.ReadShort();
	gameData.segData.nVertices = vertices;
	gameData.renderData.color.vertBright.resize(vertices);
	equal(std::size_t(vertices) == decoded.vertices.size() && std::size_t(count) == decoded.segments.size(), "Geometry counts differ");
	for (const auto &vertex : decoded.vertices)
		for (const auto value : vertex) equal(geometry.ReadFix() == value, "Vertex coordinate differs");
	std::vector<CSegment> segments(count);
	for (int i = 0; i < count; ++i) {
		auto &reference = segments[i];
		for (int side = 0; side < 6; ++side)
			for (int corner = 0; corner < 4; ++corner) reference.m_sides[side].m_corners[corner] = static_cast<uint8_t>(sideVertIndex[side][corner]);
		reference.Read(geometry);
		const auto &redux = decoded.segments[i];
		equal(reference.m_owner == redux.owner && reference.m_group == redux.group && reference.m_children == redux.children, "Segment ownership or children differ");
		std::vector<int> active;
		for (const int vertex : redux.vertices)
			if (vertex < 0xfff8) active.push_back(vertex);
		equal(active.size() == std::size_t(reference.m_nVertices), "Active vertex count differs");
		for (std::size_t n = 0; n < active.size(); ++n) equal(reference.m_vertices[n] == active[n], "Compacted vertex order differs");
		for (int side = 0; side < 6; ++side) {
			const auto &rs = reference.m_sides[side];
			const auto &ds = redux.sides[side];
			equal(rs.m_nWall == (ds.wall < 0 ? 0xffff : ds.wall), "Wall number differs");
			const int corners = static_cast<int>(std::count_if(ds.corners.begin(), ds.corners.end(), [](int n) { return n != 255; }));
			equal(rs.m_nCorners == corners, "Side shape differs");
			for (int n = 0; n < corners; ++n) equal(reference.m_vertices[rs.m_corners[n]] == redux.vertices[ds.corners[n]], "Global side vertex differs after compaction");
			const bool textured = redux.children[side] == -1 || ds.wall >= 0;
			equal(rs.m_nBaseTex == (textured ? ds.texture % 910 : 0), "Base texture normalization differs");
			equal(rs.m_nOvlTex == (textured ? (ds.overlay & 0x3fff) % 910 : 0), "Overlay texture normalization differs");
			if (textured) {
				equal(rs.m_nOvlOrient == ((ds.overlay >> 14) & 3), "Overlay orientation differs");
				for (int n = 0; n < 4; ++n) equal(rs.m_uvls[n].u == ds.uvl[n * 3] * 32 && rs.m_uvls[n].v == ds.uvl[n * 3 + 1] * 32 && rs.m_uvls[n].l == ds.uvl[n * 3 + 2] * 2, "Decoded UVL differs");
			}
			++side_count;
		}
		++segment_count;
	}
	for (int i = 0; i < count; ++i) {
		auto &reference = segments[i];
		reference.ReadExtras(geometry);
		const auto &redux = decoded.segments[i];
		equal(reference.m_function == redux.function && reference.m_nObjProducer == redux.producer && reference.m_value == redux.value && reference.m_flags == redux.flags && reference.m_props == redux.properties, "Extras field width or signedness differs");
		equal(reference.m_xDamage[0] == redux.damage[0] * 65536 && reference.m_xDamage[1] == redux.damage[1] * 65536 && reference.m_xAvgSegLight == redux.light, "Extras damage or light differs");
	}
	// Colors follow the readers under test; their widths independently establish
	// that no added corner or wide extras field was skipped or consumed twice
	equal(geometry.position + (vertices + count * 6 + 910) * 13 == std::size_t(game), "Geometry record boundary differs");
	CFile game_reader{ bytes, std::size_t(game) };
	gameFileInfo = {};
	gameTopFileInfo = {};
	equal(ReadGameFileInfo(game_reader, game) == 0, "Reference game header rejected");
	equal(gameFileInfo.fileinfoVersion == decoded.game_version, "Game version differs");
	const Section sections[] = { gameFileInfo.objects, gameFileInfo.walls, gameFileInfo.doors, gameFileInfo.triggers, gameFileInfo.links, gameFileInfo.control, gameFileInfo.botGen, gameFileInfo.lightDeltaIndices, gameFileInfo.lightDeltas, gameFileInfo.equipGen };
	for (std::size_t i = 0; i < decoded.sections.size(); ++i) equal(sections[i].offset == decoded.sections[i].offset && sections[i].count == decoded.sections[i].count && sections[i].size == decoded.sections[i].declared_size, "Game section descriptor differs");
	for (int i = 0; i < 4; ++i) {
		const auto &color = gameData.segData.fogData[i].m_color.v.color;
		if (decoded.level_version >= 27) {
			const auto &fog = decoded.fog_settings.at(i);
			equal(color.r == float(fog[0]) / 255.0f && color.g == float(fog[1]) / 255.0f && color.b == float(fog[2]) / 255.0f && color.a == float(fog[3]) * 20.0f, "Authored fog differs");
		} else {
			equal(decoded.fog_settings.empty(), "Legacy file invented serialized fog");
			const float defaults[4][4] = { { .2f, .4f, .6f, 160 }, { .8f, .4f, 0, 60 }, { .7f, .7f, .7f, 240 }, { .7f, .7f, .7f, 100 } };
			equal(color.r == defaults[i][0] && color.g == defaults[i][1] && color.b == defaults[i][2] && color.a == defaults[i][3], "Reference default fog differs");
		}
	}
	std::string name;
	for (int c; (c = game_reader.ReadUByte()) && c != '\n';) name.push_back(static_cast<char>(c));
	equal(name == decoded.level_name, "Fog boundary consumed level name");
	++level_count;
}
void original(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	check(bool(input), "Cannot open original HOG");
	const auto size = input.tellg();
	check(size >= 3, "Original HOG is empty");
	auto read = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
		check(offset <= std::uint64_t(size) && count <= std::uint64_t(size) - offset, "Original read outside HOG");
		input.clear();
		input.seekg(static_cast<std::streamoff>(offset));
		input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
		check(bool(input), "Original read failed");
	};
	const auto members = xl_compat::index_hog(static_cast<std::uint64_t>(size), read);
	for (const auto &member : members) {
		const auto name = xl_compat::hog_key(member.name);
		if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
		check(member.size <= 64 * 1024 * 1024, "Original level exceeds bounded reader limit");
		xl_compat::Bytes bytes(static_cast<std::size_t>(member.size));
		read(member.offset, bytes.data(), bytes.size());
		try {
			compare(bytes);
		} catch (const std::exception &e) {
			throw std::runtime_error(member.name + ": " + e.what());
		}
		std::cout << member.name << ": source reader agrees\n";
	}
}
int main(int argc, char **argv)
{
	try {
		if (argc > 1)
			for (int i = 1; i < argc; ++i) original(argv[i]);
		else
			for (const int version : { 23, 25, 27 })
				for (int shape = 0; shape < (version == 23 ? 1 : 3); ++shape)
					for (const int value : { -32768, -129, -128, -1, 0, 127, 128, 32767 }) {
						if (version == 23 && (value < -128 || value > 127)) continue;
						compare(xl_test::layout_fixture(version, shape, value, value).bytes);
					}
		check(level_count > 0, "No levels compared");
		std::cout << level_count << " levels, " << segment_count << " segments, " << side_count << " sides, " << comparisons << " comparisons; no runtime admission granted\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
