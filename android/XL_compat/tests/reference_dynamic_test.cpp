// Compile unchanged reference color accumulation; substitutes only data storage
#include "lighting.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace reference
{
struct CFloatVector3 {
	float rgb[3]{};
	float &Red()
	{
		return rgb[0];
	}
	float &Green()
	{
		return rgb[1];
	}
	float &Blue()
	{
		return rgb[2];
	}
};
struct CFloatVector : CFloatVector3 {
	float alpha = 1;
};
struct Flags {
	uint8_t value[1]{};
	void Clear()
	{
		value[0] = 0;
	}
	operator uint8_t *()
	{
		return value;
	}
};
struct {
	struct {
		int nLightingMethod = 0;
	} render;
} gameStates;
struct {
	struct {
		struct {
			bool bInitDynColoring = true, bStartDynColoring = true;
			uint8_t bGotGlobalDynColor = 0;
			Flags bGotDynColor;
			CFloatVector3 globalDynColor, dynamicColor[1];
		} lights;
	} renderData;
} gameData;
#include "xl_reference_dynamic.h"
} // namespace reference

int main()
{
	try {
		const xl_compat::Rgb colors[] = { { 1, 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, 0 }, { 0.25f, 0.75f, 0.5f } };
		unsigned samples = 0;
		for (unsigned sequence = 0; sequence < 1024; ++sequence) {
			xl_compat::DynamicVertexLight actual;
			reference::gameData = {};
			int scalar = 0;
			for (unsigned step = 0; step < 12; ++step) {
				const auto &color = colors[(sequence + step * 7) % 6];
				const bool force = ((sequence >> (step % 10)) & 1) != 0;
				const bool missing = (sequence + step) % 7 == 0;
				const int contribution = int((sequence * 19 + step * 101) % 10000) - 2000;
				actual.add(contribution, missing ? nullptr : &color, force);
				scalar += contribution;
				reference::CFloatVector input;
				for (unsigned i = 0; i < 3; ++i) input.rgb[i] = color[i];
				reference::SetDynColor(missing ? nullptr : &input, nullptr, 0, nullptr, force);
				const auto &expected = reference::gameData.renderData.lights;
				if (actual.intensity != scalar || actual.has_color != bool(expected.bGotDynColor.value[0]))
					throw std::runtime_error("dynamic state differs from XL reference");
				for (unsigned i = 0; i < 3; ++i)
					if (actual.color[i] != expected.dynamicColor[0].rgb[i])
						throw std::runtime_error("source-order color differs from XL reference");
				++samples;
			}
		}
		std::cout << "XL dynamic color: " << samples << " reference comparisons passed; emitters/attenuation not covered\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
