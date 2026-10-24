// Controlled persistence evidence only; never used by normal pilot gameplay
#include "persistence_probe.h"
#include "preview.h"
#include <cstring>
#include <vector>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "wall.h"
#include "lighting.h"
}

namespace
{
std::vector<fix> saved_uvls;
std::vector<int> saved_subtracted;
int light_segment = -1, light_side = -1;
bool disturbed = false;
std::vector<fix> face_lights()
{
	std::vector<fix> values;
	for (int seg = 0; seg < Num_segments; ++seg)
		for (int side = 0; side < 6; ++side)
			for (int corner = 0; corner < 4; ++corner)
				values.push_back(Segments[seg].sides[side].uvls[corner].l);
	return values;
}
} // namespace

const char *xl_pilot_persistence_probe(const char *operation)
{
	if (!xl_preview_pilot_active() || Num_segments != 228 || Num_walls != 86 || Num_static_lights != 70)
		return "XL persistence fixture requires the admitted original pilot";
	if (!std::strcmp(operation, "prepare")) {
		saved_uvls.clear();
		saved_subtracted.clear();
		disturbed = false;
		if (Walls[6].type != WALL_ILLUSION || Walls[6].cloak_value != 21)
			return "XL persistence fixture requires fresh authored opacity";
		light_segment = Dl_indices[0].segnum;
		light_side = Dl_indices[0].sidenum;
		const auto before = face_lights();
		if (!subtract_light(light_segment, light_side) || face_lights() == before)
			return "XL persistence fixture did not change authored UVLs";
		Walls[6].cloak_value = 11;
		saved_uvls = face_lights();
		saved_subtracted.assign(Light_subtracted, Light_subtracted + Num_segments);
		return nullptr;
	}
	if (saved_uvls.empty()) return "XL persistence fixture was not prepared";
	if (!std::strcmp(operation, "disturb")) {
		if (Walls[6].cloak_value != 11 || face_lights() != saved_uvls)
			return "XL persistence fixture changed before the deliberate disturbance";
		Walls[6].cloak_value = 30;
		if (!add_light(light_segment, light_side) || face_lights() == saved_uvls)
			return "XL persistence fixture did not restore the light for its negative control";
		disturbed = true;
		return nullptr;
	}
	if (std::strcmp(operation, "verify")) return "Unknown XL persistence operation";
	if (!disturbed) return "XL persistence fixture lacks its negative control";
	if (Walls[6].cloak_value != 11 || face_lights() != saved_uvls ||
	    std::vector<int>(Light_subtracted, Light_subtracted + Num_segments) != saved_subtracted)
		return "XL restore lost authored wall opacity or switched-light state";
	xl_preview_begin_face(Walls[6].segnum, Walls[6].sidenum);
	const float alpha = xl_preview_alpha(1);
	xl_preview_end_face();
	if (alpha != 20.0f / 31.0f) return "XL restored opacity did not reach the material adapter";
	return nullptr;
}
