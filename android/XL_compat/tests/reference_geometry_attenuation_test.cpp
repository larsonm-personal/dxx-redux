#include "lighting.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

static std::optional<xl_compat::GeometryAttenuation> reference(const xl_compat::GeometryLightSample &input)
{
	struct {
		struct {
			float fLightRange;
		} m_states;
	} ogl{ { input.range_scale } };
	struct {
		struct {
			float fBrightness;
		} info;
	} light{ { input.brightness } };
	const auto *pLight = &light;
	float fLightDist = input.distance, NdotL = input.normal_dot, fLightAngle = input.emission_angle, fAttenuation = 0;
#define Min              std::min
#define ATTENUATION_TYPE 0
#define MAX_LIGHT_DIST   input.cutoff
#define GEO_LIN_ATT      input.linear_attenuation
#define GEO_QUAD_ATT     input.quadratic_attenuation
	do {
#include "xl_reference_geometry_attenuation.h"
		return xl_compat::GeometryAttenuation{ fLightDist, NdotL, fAttenuation };
	} while (false);
#undef Min
#undef ATTENUATION_TYPE
#undef MAX_LIGHT_DIST
#undef GEO_LIN_ATT
#undef GEO_QUAD_ATT
	return std::nullopt;
}

int main()
{
	try {
		unsigned checked = 0;
		for (float distance : { 0.0f, 0.1f, 1.0f, 16.0f, 222.0f, 1000.0f })
			for (float angle : { -1.0f, -0.5f, -0.01f, 0.0f, 0.5f, 1.0f })
				for (float normal : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
					for (float range : { 0.5f, 1.0f, 2.0f })
						for (float cutoff : { 222.0f, 222.0f * 65536 })
							for (float brightness : { 0.1f, 1.0f, 2.0f }) {
								xl_compat::GeometryLightSample input{ distance, normal, angle, range, 0.05f, 0.005f, cutoff, brightness };
								const auto actual = xl_compat::geometry_attenuation(input), expected = reference(input);
								if (bool(actual) != bool(expected)) throw std::runtime_error("geometry cutoff differs from pinned reference");
								if (actual && (actual->distance != expected->distance || actual->normal_dot != expected->normal_dot || actual->attenuation != expected->attenuation))
									throw std::runtime_error("geometry attenuation arithmetic differs from pinned reference");
								++checked;
							}
		std::cout << checked << " geometry attenuation cases match pinned source; visibility and shading unqualified\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
