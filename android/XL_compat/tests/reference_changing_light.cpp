#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using fix = int32_t;
constexpr int I2X(int value)
{
	return value * 65536;
}
constexpr int DL_SCALE = 2048, WID_TRANSPARENT_FLAG = 4;
constexpr int LIGHT_DISTANCE_THRESHOLD = I2X(80), MAGIC_LIGHT_CONSTANT = I2X(16);
#define DBG 0
fix FixDiv(fix a, fix b)
{
	return static_cast<fix>(int64_t(a) * 65536 / b);
}
fix FixMul(fix a, fix b)
{
	const int64_t product = int64_t(a) * b;
	return static_cast<fix>(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
}
struct tUVL {
	fix l = 0;
};
struct CSide {
	tUVL m_uvls[4];
	int CornerCount() const
	{
		return 4;
	}
};
int sample_distance;
struct CFixVector {
	static fix Dist(const CFixVector &, const CFixVector &)
	{
		return sample_distance;
	}
};
struct CSegment {
	CSide sides[6];
	fix m_xAvgSegLight = 0;
	int m_children[6]{};
	int16_t Index() const
	{
		return 0;
	}
	CFixVector Center() const
	{
		return {};
	}
	int IsPassable(int, void *) const
	{
		return 0;
	}
	CSide *Side(int side)
	{
		return &sides[side];
	}
} segment;
CSegment *SEGMENT(int)
{
	return &segment;
}
struct CLightDeltaIndex {
	int16_t nSegment;
	uint16_t nSide : 3;
	uint16_t count : 13;
	uint16_t index;
};
struct CLightDelta {
	int16_t nSegment = 0, nSide = 0;
	bool bValid = true;
	int vertLight[4]{};
};
template <class T>
struct Array {
	std::vector<T> data;
	std::size_t start = 0;
	T *Buffer()
	{
		return data.empty() ? nullptr : data.data() + start;
	}
	T *operator+(int index)
	{
		return Buffer() + index;
	}
	int Length() const
	{
		return static_cast<int>(data.size() - start);
	}
};
struct {
	struct {
		struct {
			Array<CLightDeltaIndex> deltaIndices;
			Array<CLightDelta> deltas;
			Array<fix> segDeltas;
			int nStatic = 0;
		} lights;
	} renderData;
} gameData;
struct {
	struct {
		int nLightingMethod = 0;
	} render;
	struct {
		bool bNostalgia = false;
	} app;
} gameStates;
struct {
	bool Delete(int, int, int)
	{
		return false;
	}
	int Toggle(int, int, int, bool)
	{
		return -1;
	}
} lightManager;
void ChangeSegmentLight(int16_t, int16_t, int32_t) {}
int16_t changedSegs[30];
int32_t nChangedSegs;
#include "xl_reference_changing_light.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected production light sample path");
		std::ifstream input(argv[1]);
		if (!input) throw std::runtime_error("Cannot open production light samples");
		int deltas = 0, segments = 0;
		char kind;
		while (input >> kind) {
			if (kind == 'D') {
				int base, value, direction, count, expected[4];
				input >> base >> value >> direction >> count;
				for (auto &v : expected) input >> v;
				auto &lights = gameData.renderData.lights;
				// Pad before logical Buffer: source duplicate rewind reads one earlier row
				// The production lookup has a checked lower bound instead
				lights.deltaIndices.data.assign(4, {});
				lights.deltaIndices.start = 1;
				lights.deltaIndices.data[0].nSegment = -1;
				lights.deltaIndices.data[1].count = static_cast<uint16_t>(count);
				lights.nStatic = 3;
				lights.deltas.data.assign(count, {});
				lights.segDeltas.data.assign(6, 0);
				for (auto &d : lights.deltas.data) {
					d.vertLight[0] = value;
					d.vertLight[1] = value / 2;
					d.vertLight[3] = value % 3;
				}
				for (auto &uvl : segment.sides[0].m_uvls) uvl.l = base;
				ChangeLight(0, 0, direction);
				for (int c = 0; c < 4; ++c)
					if (segment.sides[0].m_uvls[c].l != expected[c]) throw std::runtime_error("Source UVL sample differs");
				++deltas;
			} else if (kind == 'S') {
				int brightness, expected;
				input >> sample_distance >> brightness >> expected;
				nChangedSegs = 0;
				segment.m_xAvgSegLight = I2X(2);
				CFixVector origin;
				ApplyLightToSegment(&segment, &origin, brightness, 0);
				if (segment.m_xAvgSegLight - I2X(2) != expected) throw std::runtime_error("Source segment attenuation differs");
				++segments;
			} else throw std::runtime_error("Unknown light sample kind");
			if (!input) throw std::runtime_error("Truncated production light sample");
		}
		if (deltas < 100 || segments != 56) throw std::runtime_error("Missing light reference coverage");
		std::cout << deltas << " source UVL and " << segments << " segment attenuation samples matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
