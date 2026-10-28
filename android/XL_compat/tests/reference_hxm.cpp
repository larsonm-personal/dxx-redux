#include "../model_geometry.h"
#include "../inventory.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

using fix = int32_t;
struct CFixVector {
	struct {
		struct {
			int32_t x, y, z;
		} coord;
	} v;
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
};
struct tHitbox {
	CFixVector vMin, vMax, vOffset, vSize;
};
constexpr int MAX_HITBOXES = 64;
struct {
	struct {
		struct {
			tHitbox hitboxes[MAX_HITBOXES + 1];
			int nHitboxes;
		} hitboxes[1];
	} modelData;
} gameData;
class CPolyModel
{
  public:
	struct {
		int nId = 0;
	} m_info;
	std::vector<uint8_t> data;
	void *Buffer()
	{
		return data.data();
	}
	fix Size();
};
enum { OP_EOF,
	   OP_DEFPOINTS,
	   OP_FLATPOLY,
	   OP_TMAPPOLY,
	   OP_SORTNORM,
	   OP_RODBM,
	   OP_SUBCALL,
	   OP_DEFP_START,
	   OP_GLOW };
static int WORDVAL(const uint8_t *p)
{
	return p[0] | unsigned(p[1]) << 8;
}
#define VECPTR(p) reinterpret_cast<CFixVector *>(p)
static void G3CheckAndSwap(void *) {}
static bool facing;
static bool G3CheckNormalFacing(const CFixVector &, const CFixVector &)
{
	return facing;
}
static int G3ModelMinMax(int, tHitbox *)
{
	return 0;
}
static void ComputeHitbox(int, int) {}
static void Error(const char *message)
{
	throw std::runtime_error(message);
}
#include "xl_reference_hxm_geometry.h"
#undef VECPTR

static void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
static void word(std::vector<uint8_t> &bytes, unsigned value)
{
	bytes.push_back(uint8_t(value));
	bytes.push_back(uint8_t(value >> 8));
}
static void integer(std::vector<uint8_t> &bytes, int32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(uint32_t(value) >> shift));
}
static int compare(const std::vector<uint8_t> &bytes)
{
	const int actual = xl_compat::ModelGeometry(bytes.data(), bytes.size()).custom_radius();
	for (bool direction : { false, true }) {
		facing = direction;
		CPolyModel source;
		source.data = bytes;
		require(source.Size() == actual, "HXM custom radius differs from extracted XL functions");
	}
	return actual;
}
static std::vector<uint8_t> points()
{
	std::vector<uint8_t> bytes;
	word(bytes, OP_DEFP_START);
	word(bytes, 4);
	word(bytes, 0);
	word(bytes, 0);
	for (int value : { 0, -65536, 65536, 0 })
		for (int axis = 0; axis < 3; ++axis) integer(bytes, value * (axis + 1));
	word(bytes, OP_EOF);
	return bytes;
}
static unsigned read32(const std::vector<uint8_t> &bytes, std::size_t &at)
{
	require(at <= bytes.size() && bytes.size() - at >= 4, "HXM test input is truncated");
	unsigned result = 0;
	for (unsigned shift = 0; shift < 32; shift += 8) result |= unsigned(bytes[at++]) << shift;
	return result;
}
static void original_models(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	require(bool(file), "Original HXM archive is unavailable");
	const xl_compat::Bytes hog{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	unsigned tested = 0;
	for (const auto &member : xl_compat::read_hog(hog)) {
		std::string name = member.name;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		if (name != "alarlof.hxm") continue;
		const auto &bytes = member.bytes;
		std::size_t at = 0;
		require(read32(bytes, at) == 0x21584d48 && read32(bytes, at) == 1, "Original HXM header changed");
		const auto robots = read32(bytes, at);
		at += robots * 484;
		const auto joints = read32(bytes, at);
		at += joints * 12;
		const auto models = read32(bytes, at);
		for (unsigned model = 0; model < models; ++model) {
			const auto id = read32(bytes, at);
			auto header = at + 4;
			const auto size = read32(bytes, header);
			at += 734;
			require(at <= bytes.size() && size <= bytes.size() - at, "Original HXM model truncated");
			const std::vector<uint8_t> payload(bytes.begin() + at, bytes.begin() + at + size);
			std::cout << "Alarlof model " << id << " custom radius " << compare(payload) << '\n';
			at += size + 8;
			++tested;
		}
	}
	require(tested == 1, "Original Alarlof model count changed");
}
int main(int argc, char **argv)
{
	try {
		const auto cloud = points();
		compare(cloud);
		auto nested = cloud;
		nested.resize(nested.size() - 2);
		word(nested, OP_SUBCALL);
		word(nested, 1);
		for (int offset : { 131072, -65536, 262144 }) integer(nested, offset);
		word(nested, 22);
		word(nested, 0);
		word(nested, OP_EOF);
		nested.insert(nested.end(), cloud.begin(), cloud.end());
		compare(nested);
		for (int variant = 0; variant < 2; ++variant) {
			auto bad = nested;
			const auto subcall = cloud.size() - 2;
			if (!variant) bad[subcall + 2] = 10;
			else {
				for (int byte = 0; byte < 4; ++byte) bad[subcall + 4 + byte] = uint8_t(uint32_t(INT32_MAX) >> (byte * 8));
			}
			bool rejected = false;
			try {
				xl_compat::ModelGeometry(bad.data(), bad.size()).custom_radius();
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Invalid animation submodel or transformed coordinate accepted");
		}
		auto sorted = std::vector<uint8_t>(32, 0);
		sorted[0] = OP_SORTNORM;
		sorted[28] = 34;
		sorted[30] = uint8_t(34 + cloud.size());
		word(sorted, OP_EOF);
		sorted.insert(sorted.end(), cloud.begin(), cloud.end());
		sorted.insert(sorted.end(), cloud.begin(), cloud.end());
		compare(sorted);
		for (auto bad : { std::vector<uint8_t>{}, std::vector<uint8_t>{ 99, 0 }, std::vector<uint8_t>{ OP_SUBCALL, 0 } }) {
			bool rejected = false;
			try {
				xl_compat::ModelGeometry geometry(bad.data(), bad.size());
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Malformed model geometry accepted");
		}
		// XL's else-if extrema updates leave max uninitialized for decreasing points
		// The bounded reader defines both extrema, rather than reproducing overflow
		std::vector<uint8_t> monotone;
		word(monotone, OP_DEFPOINTS);
		word(monotone, 2);
		for (int value : { 65536, -65536 })
			for (int axis = 0; axis < 3; ++axis) integer(monotone, value);
		word(monotone, OP_EOF);
		require(xl_compat::ModelGeometry(monotone.data(), monotone.size()).custom_radius() == 92682, "Monotone vertex correction changed");
		std::vector<uint8_t> minimum;
		word(minimum, OP_DEFPOINTS);
		word(minimum, 2);
		for (int value : { INT32_MIN, INT32_MIN + 2 })
			for (int axis = 0; axis < 3; ++axis) integer(minimum, value);
		word(minimum, OP_EOF);
		require(xl_compat::ModelGeometry(minimum.data(), minimum.size()).custom_radius() == 2, "Minimum signed coordinate extrema changed");
		if (argc > 1) original_models(argv[1]);
		std::cout << "HXM extracted-source geometry, nested offsets, sort branches and bounded malformed controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
