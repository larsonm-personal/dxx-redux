// Shared original-file diagnostic for the desktop lifecycle owner and Android automation
#ifndef XL_ORIGINAL_LIGHTING_TEST_H
#define XL_ORIGINAL_LIGHTING_TEST_H
#include "companion_resources.h"
#include "preview.h"
#include "resource_validation.h"
#include <array>
#include <nlohmann/json.hpp>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "gameseg.h"
}

inline nlohmann::json xl_original_lighting_face(int segment, int side)
{
	if (segment < 0 || segment >= Num_segments || side < 0 || side >= 6) throw std::runtime_error("Lighting sample outside loaded geometry");
	const auto &face = Segments[segment].sides[side];
	const auto texture = (face.tmap_num2 & 0x3fff) ? (face.tmap_num2 & 0x3fff) : face.tmap_num;
	int brightness = 0;
	nlohmann::json sample{ { "segment", segment }, { "side", side }, { "texture", texture }, { "subtracted", bool(Light_subtracted[segment] & (1 << side)) } };
	if (xl_prepared_texture_brightness(texture, &brightness)) sample["brightness"] = brightness;
	if (const auto *lighting = xl_prepared_classic_lighting()) {
		sample["raw_rgb"] = lighting->colors->at(texture).channels;
		sample["sources"] = lighting->sources;
	}
	xl_texture_emitter emitter{};
	if (xl_preview_texture_emitter(segment, side, &emitter))
		sample["emitter"] = { { "texture", emitter.texture }, { "rgb", { emitter.rgb[0], emitter.rgb[1], emitter.rgb[2] } }, { "intensity", emitter.intensity } };
	int vertices[4];
	get_side_verts(vertices, segment, side);
	for (int corner = 0; corner < 4; ++corner) {
		const int light = face.uvls[corner].l;
		std::array<int32_t, 3> shaded{ light, light, light };
		xl_preview_vertex(vertices[corner], light, &shaded[0], &shaded[1], &shaded[2]);
		sample["shaded"].push_back(shaded);
	}
	return sample;
}
#endif
