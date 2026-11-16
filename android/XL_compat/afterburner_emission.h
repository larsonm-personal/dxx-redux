#ifndef DXX_XL_AFTERBURNER_EMISSION_H
#define DXX_XL_AFTERBURNER_EMISSION_H
#include <array>
#include <cstdint>
#include <stdexcept>
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{
using EmissionPoint = std::array<int32_t, 3>;
struct AfterburnerModel {
	bool ase = false;
	int32_t low_z = 0, high_z = 0;
};
struct AfterburnerEmission {
	std::array<EmissionPoint, 2> positions{};
	unsigned count = 0;
};
// CalcPos(..., bAfterburnerBlob=1), with model selection owned by the caller
inline AfterburnerEmission afterburner_emission(const EmissionPoint &position, const EmissionPoint &forward,
                                                const EmissionPoint &right, int32_t size, bool player,
                                                bool hires, bool ase, int32_t model_low_z, int32_t model_high_z)
{
	const auto checked = [](int64_t value) {
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL afterburner emission coordinate overflows");
		return int32_t(value);
	};
	const auto advance = [&](EmissionPoint &point, const EmissionPoint &direction, int32_t distance) {
		for (unsigned axis = 0; axis < 3; ++axis)
			point[axis] = checked(int64_t(point[axis]) + ((int64_t(direction[axis]) * distance) >> 16));
	};
	AfterburnerEmission result;
	result.positions[0] = position;
	if (hires && player && !ase) {
		if (size < 0 || int64_t(size) * 8 > INT32_MAX) throw std::runtime_error("XL afterburner ship size is invalid");
		advance(result.positions[0], forward, -size);
		advance(result.positions[0], right, -(8 * size / 44));
		result.positions[1] = result.positions[0];
		advance(result.positions[1], right, 8 * size / 22);
		result.count = 2;
	} else {
		const auto span = int64_t(model_high_z) - model_low_z;
		if (span < 0 || span > INT32_MAX) throw std::runtime_error("XL afterburner model extent is invalid");
		advance(result.positions[0], forward, -int32_t(span / 2 * 2));
		result.count = 1;
	}
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
