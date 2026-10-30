// Actual source attenuation and trigonometry, with distance and ear-angle adapters
#include "audio_spatial.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace spatial_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN return;
#include "xl_reference_audio_constants.h"
int selected_distance = 0;
std::int16_t selected_angle = 0;
struct CFixVector {
	static fix DeltaAngleNorm(const CFixVector &, const CFixVector &, CFixVector *)
	{
		return selected_angle;
	}
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u;
		} dir;
	} m;
};
struct {
	int bReverseChannels = 0;
} gameConfig;
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
// The table profile is deliberately unselected; only default math format 2 is qualified
const std::int16_t sincos_table[321]{};
struct CAudio {
	int Distance(CFixVector &, int16_t, CFixVector &, int16_t, fix, int32_t, CFixVector &, int32_t)
	{
		return selected_distance;
	}
	void GetVolPan(CFixMatrix &, CFixVector &, int16_t, CFixVector &, int16_t, fix, int32_t *, int32_t *, fix, int32_t, int32_t);
};
#include "xl_reference_audio_spatial.h"
} // namespace spatial_reference
void audio_spatial_source_controls()
{
	using namespace spatial_reference;
	CAudio source;
	CFixVector point;
	CFixMatrix orientation;
	unsigned comparisons = 0;
	const auto compare = [&](int distance, int volume, int range, int decay, int angle, int reverse, int hires) {
		selected_distance = distance;
		selected_angle = std::int16_t(angle);
		gameConfig.bReverseChannels = reverse;
		options.hires = hires;
		int expected_volume = -1, expected_pan = -1;
		source.GetVolPan(orientation, point, 0, point, 1, volume, &expected_volume, &expected_pan, range, decay, 0);
		const auto actual = xl_compat::audio_spatial_output(distance, volume, range, decay, selected_angle, reverse != 0, hires);
		if (actual.volume != expected_volume || actual.pan != expected_pan) {
			std::cerr << "Spatial mismatch distance=" << distance << " volume=" << volume << " range=" << range << " decay=" << decay << " angle=" << angle << " reverse=" << reverse << " hires=" << hires << " expected=" << expected_volume << "," << expected_pan << " actual=" << actual.volume << "," << actual.pan << "\n";
			throw std::runtime_error("Owned spatial volume or pan differs from source");
		}
		++comparisons;
	};
	for (int angle = -32768; angle <= 32767; ++angle)
		for (int reverse : { 0, 1 })
			for (int hires : { 0, 1, 2 }) compare(1, 65536, 256 * 65536, 0, angle, reverse, hires);
	for (int range : { 1, 2, 3, 65535, 65536, 256 * 65536 })
		for (int distance : { -1, 0, 1, range / 2, range - 1, range, range + 1, 2 * range - 1 })
			for (int volume : { 0, 1, 65536, 4 * 65536, 10 * 65536 })
				for (int decay : { 0, 1, 2, 3, -1 }) {
					if (range < 2 && decay == 1) continue;
					compare(distance, volume, range, decay, 12345, 0, 1);
				}
	std::cout << "PASS: " << comparisons << " source spatial attenuation and pan comparisons; routing and ear-angle geometry are independent inputs\n";
}
