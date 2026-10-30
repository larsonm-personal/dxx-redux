// Actual source vector math, inverse cosine and final GetVolPan output
#include "audio_spatial.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
namespace ear_reference
{
using fix = std::int32_t;
using fixang = std::int16_t;
#define ENTER(a, b)
#define RETURN return;
#include "xl_reference_audio_constants.h"
struct Options {
	struct {
		int nMathFormat = 2;
	} render;
	int hires = 1;
	int UseHiresSound() const
	{
		return hires;
	}
} options;
auto *gameOpts = &options;
struct {
	int bReverseChannels = 0;
} gameConfig;
struct tQuadInt {
	std::uint32_t low;
	std::int32_t high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected integer vector profile");
}
fix QuadSqrt(std::uint32_t, std::int32_t)
{
	throw std::runtime_error("Unexpected integer vector profile");
}
std::int64_t mul64(fix a, fix b)
{
	return std::int64_t(a) * b;
}
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	static CFixVector Create(fix x, fix y, fix z)
	{
		CFixVector r;
		r.v.coord = { x, y, z };
		return r;
	}
	CFixVector operator-(const CFixVector &b) const
	{
		return Create(v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y, v.coord.z - b.v.coord.z);
	}
	fix Mag() const;
	static const CFixVector Cross(const CFixVector &, const CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	static const fix NormalizedDir(CFixVector &, const CFixVector &, const CFixVector &);
	static const fixang DeltaAngleNorm(const CFixVector &, const CFixVector &, CFixVector *);
};
#include "xl_reference_audio_geometry.h"
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u;
		} dir;
	} m;
};
const std::int16_t sincos_table[321]{};
int routed_distance = -2;
struct CAudio {
	// Same-segment Distance exits directly after actual NormalizedDir
	int Distance(CFixVector &listener, int16_t, CFixVector &sound, int16_t, fix, int32_t, CFixVector &direction, int32_t)
	{
		const auto direct = CFixVector::NormalizedDir(direction, sound, listener);
		return routed_distance == -2 ? direct : routed_distance;
	}
	void GetVolPan(CFixMatrix &, CFixVector &, int16_t, CFixVector &, int16_t, fix, int32_t *, int32_t *, fix, int32_t, int32_t);
};
#include "xl_reference_audio_spatial.h"
} // namespace ear_reference
xl_compat::AudioSpatialOutput audio_geometry_reference_output(int distance, const xl_compat::FixedLightPoint &right, const xl_compat::FixedLightPoint &up, const xl_compat::FixedLightPoint &listener, const xl_compat::FixedLightPoint &sound, int volume, int range, int decay, bool reverse, int hires)
{
	using namespace ear_reference;
	const auto vector = [](const xl_compat::FixedLightPoint &p) { return CFixVector::Create(p[0], p[1], p[2]); };
	CAudio source;
	CFixMatrix orientation;
	orientation.m.dir.r = vector(right);
	orientation.m.dir.u = vector(up);
	auto a = vector(listener), b = vector(sound);
	routed_distance = distance;
	gameConfig.bReverseChannels = reverse;
	options.hires = hires;
	xl_compat::AudioSpatialOutput result;
	source.GetVolPan(orientation, a, 0, b, 1, volume, &result.volume, &result.pan, range, decay, 0);
	routed_distance = -2;
	gameConfig.bReverseChannels = 0;
	options.hires = 1;
	return result;
}
void audio_geometry_source_controls()
{
	using namespace ear_reference;
	unsigned comparisons = 0;
	const auto point = [](const CFixVector &v) { return xl_compat::FixedLightPoint{ v.v.coord.x, v.v.coord.y, v.v.coord.z }; };
	const auto compare = [&](CFixVector right, CFixVector up, CFixVector listener, CFixVector sound) {
		CFixVector direction;
		const auto distance = CFixVector::NormalizedDir(direction, sound, listener);
		const auto expected = CFixVector::DeltaAngleNorm(right, direction, &up);
		const auto actual = xl_compat::audio_ear_angle(point(right), point(up), point(listener), point(sound));
		if (expected != actual) throw std::runtime_error("Owned ear angle differs from source vector math");
		CAudio source;
		CFixMatrix orientation;
		orientation.m.dir.r = right;
		orientation.m.dir.u = up;
		int volume = -1, pan = -1;
		source.GetVolPan(orientation, listener, 0, sound, 0, 65536, &volume, &pan, 256 * 65536, 0, 0);
		const auto output = xl_compat::audio_spatial_output(distance, 65536, 256 * 65536, 0, actual, false, 1);
		if (volume != output.volume || pan != output.pan) throw std::runtime_error("Owned orientation-to-pan output differs from source");
		++comparisons;
	};
	const auto zero = CFixVector::Create(0, 0, 0);
	const auto x = CFixVector::Create(65536, 0, 0), y = CFixVector::Create(0, 65536, 0);
	// Every interpolated inverse-cosine input and both exact unit boundaries
	for (int dot = -65537; dot <= 65537; ++dot) compare(CFixVector::Create(dot, 0, 0), y, zero, x);
	for (int a : { -65536, -1, 0, 1, 65536 })
		for (int b : { -65536, -1, 0, 1, 65536 })
			for (int c : { -65536, -1, 0, 1, 65536 })
				compare(x, y, zero, CFixVector::Create(a, b, c));
	std::mt19937 random(0x584c4541);
	const auto vector = [&]() { return CFixVector::Create(int(random() % 2000001) - 1000000, int(random() % 2000001) - 1000000, int(random() % 2000001) - 1000000); };
	for (unsigned i = 0; i < 50000; ++i) {
		auto right = vector(), up = vector();
		CFixVector::Normalize(right);
		CFixVector::Normalize(up);
		compare(right, up, vector(), vector());
	}
	std::cout << "PASS: " << comparisons << " source ear-angle and orientation-to-pan comparisons, inverse-cosine interpolation/boundaries, zero distance and arbitrary listener axes\n";
}
