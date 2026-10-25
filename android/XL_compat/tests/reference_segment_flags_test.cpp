// Minimal geometry/texture adapter; flag updates and propagation are original XL code
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

constexpr int MAX_SEGMENTS_D2X = 2, SEGMENT_SIDE_COUNT = 6, AMBIENT_SEGMENT_DEPTH = 5;
constexpr int S2F_AMBIENT_WATER = 1, S2F_AMBIENT_LAVA = 2;
constexpr int TMI_VOLATILE = 1, TMI_WATER = 2, WID_TRANSPARENT_FLAG = 4;
struct CSide {
	int m_nBaseTex = 0, m_nOvlTex = 0, m_nWall = -1;
	int faces = 0;
	int FaceCount() const
	{
		return faces;
	}
};
struct CSegment {
	uint8_t m_flags = 0;
	int m_children[6] = { -1, -1, -1, -1, -1, -1 };
	CSide m_sides[6];
	int passable = 0;
	int IsPassable(int, void *) const
	{
		return passable;
	}
};
struct {
	CSegment values[2];
	CSegment *Buffer()
	{
		return values;
	}
} SEGMENTS;
CSegment *SEGMENT(int i)
{
	return &SEGMENTS.values[i];
}
bool IS_CHILD(int i)
{
	return i >= 0;
}
bool IS_WALL(int i)
{
	return i >= 0;
}
struct {
	struct {
		int nLastSegment = 1;
	} segData;
	struct {
		struct {
			struct {
				int flags;
			} pTexMapInfo[4] = { { 0 }, { TMI_VOLATILE }, { TMI_WATER }, { TMI_VOLATILE | TMI_WATER } };
		} tex;
	} pigData;
} gameData;

#include "xl_reference_segment_flags.h"

int main()
{
	try {
		for (int flags = 0; flags < 256; ++flags)
			for (int texture = 0; texture < 4; ++texture)
				for (int passage = 0; passage < 2; ++passage) {
					SEGMENTS = {};
					SEGMENT(0)->m_flags = static_cast<uint8_t>(flags);
					SEGMENT(1)->m_flags = static_cast<uint8_t>(255 - flags);
					SEGMENT(0)->m_sides[0].faces = 1;
					SEGMENT(0)->m_sides[0].m_nBaseTex = texture;
					SEGMENT(0)->m_children[1] = 1;
					SEGMENT(1)->m_children[1] = 0;
					SEGMENT(0)->passable = SEGMENT(1)->passable = passage ? WID_TRANSPARENT_FLAG : 0;
					SetAmbientSoundFlags();
					const int ambient = (texture & TMI_WATER ? S2F_AMBIENT_WATER : 0) | (texture & TMI_VOLATILE ? S2F_AMBIENT_LAVA : 0);
					if (SEGMENT(0)->m_flags != ((flags & 0xfc) | ambient) ||
					    SEGMENT(1)->m_flags != (((255 - flags) & 0xfc) | (passage ? ambient : 0)))
						throw std::runtime_error("reference changed reserved bits or used them for ambient propagation");
				}
		std::cout << "Pinned XL ambient flags: all 256 bytes, four source masks and open/closed propagation pass\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
