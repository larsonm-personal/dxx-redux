#include "changing_light.h"
#include "preview.h"
#include "../inventory.h"
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "textures.h"
extern object *old_viewer;
}

namespace
{
const xl_compat::Inventory *authored = nullptr;
std::vector<std::size_t> ordered;

void spread(int segment, const vms_vector &origin, fix brightness, int depth, std::vector<unsigned char> &changed)
{
	if (!changed[segment]) {
		changed[segment] = 1;
		vms_vector center;
		compute_segment_center(&center, &Segments[segment]);
		const fix distance = vm_vec_dist(&center, &origin);
		if (distance <= 80 * F1_0) {
			const auto change = xl_light_at_distance(distance, brightness);
			const auto total = static_cast<int64_t>(Segment2s[segment].static_light) + change;
			Segment2s[segment].static_light = static_cast<fix>((std::max) (INT64_C(0), (std::min) (total, static_cast<int64_t>(INT32_MAX))));
		}
	}
	if (depth < 2)
		for (int side = 0; side < 6; ++side) {
			const int child = Segments[segment].children[side];
			if (child >= 0 && child < Num_segments && (WALL_IS_DOORWAY(&Segments[segment], side) & WID_RENDPAST_FLAG))
				spread(child, origin, brightness, depth + 1, changed);
		}
}

void segment_light(int segment, int side, int direction)
{
	if (WALL_IS_DOORWAY(&Segments[segment], side) & WID_RENDER_FLAG) {
		const auto &face = Segments[segment].sides[side];
		const auto brightness = (static_cast<int64_t>(TmapInfo[face.tmap_num].lighting) + TmapInfo[face.tmap_num2 & 0x3fff].lighting) * direction;
		if (brightness) {
			vms_vector center;
			compute_segment_center(&center, &Segments[segment]);
			std::vector<unsigned char> changed(Num_segments);
			spread(segment, center, static_cast<fix>((std::max) (static_cast<int64_t>(INT32_MIN), (std::min) (brightness, static_cast<int64_t>(INT32_MAX)))), 0, changed);
		}
	}
	old_viewer = nullptr;
}
} // namespace

extern "C" int xl_light_at_distance(int distance, int brightness)
{
	if (distance < 0 || distance > 80 * F1_0) return 0;
	const fix attenuation = distance > F1_0 ? fixdiv(16 * F1_0, distance) : 16 * F1_0;
	const int64_t product = static_cast<int64_t>(attenuation) * brightness;
	// Source fixed multiply rounds negative products down, not toward zero
	const auto value = product >= 0 ? product / F1_0 : -((-product + F1_0 - 1) / F1_0);
	return static_cast<int>((std::max) (-static_cast<int64_t>(F1_0 - 1), (std::min) (value, static_cast<int64_t>(F1_0 - 1))));
}

void xl_changing_light_bind(const xl_compat::Inventory *source)
{
	if (!source || source->segments.size() != static_cast<std::size_t>(Num_segments))
		throw std::runtime_error("XL changing-light geometry does not match its source");
	std::vector<std::size_t> indices(source->light_indices.size());
	std::iota(indices.begin(), indices.end(), 0);
	std::stable_sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
		const auto &x = source->light_indices[a], &y = source->light_indices[b];
		return x[0] < y[0] || (x[0] == y[0] && x[1] < y[1]);
	});
	ordered.swap(indices);
	authored = source;
}

extern "C" void xl_changing_light_clear(void)
{
	authored = nullptr;
	ordered.clear();
}

extern "C" int xl_light_index_count(void)
{
	return authored ? static_cast<int>(authored->light_indices.size()) : 0;
}

extern "C" int xl_light_delta_count(void)
{
	return authored ? static_cast<int>(authored->delta_lights.size()) : 0;
}

extern "C" int xl_light_segment_count(void)
{
	return authored ? static_cast<int>(authored->segments.size()) : 0;
}

extern "C" int xl_light_segment_get(int segment, xl_light_segment_state *out)
{
	if (!out || segment < 0 || segment >= xl_light_segment_count()) return 0;
	for (int side = 0; side < 6; ++side)
		for (int corner = 0; corner < 4; ++corner)
			out->light[side * 4 + corner] = Segments[segment].sides[side].uvls[corner].l;
	out->ambient = Segment2s[segment].static_light;
	return 1;
}

extern "C" int xl_light_segment_restore(const xl_light_segment_state *states, int count, int apply)
{
	if (!authored || !states || count != Num_segments || count != xl_light_segment_count()) return 0;
	if (apply)
		for (int segment = 0; segment < count; ++segment) {
			for (int side = 0; side < 6; ++side)
				for (int corner = 0; corner < 4; ++corner)
					Segments[segment].sides[side].uvls[corner].l = states[segment].light[side * 4 + corner];
			Segment2s[segment].static_light = states[segment].ambient;
		}
	return 1;
}

extern "C" int xl_light_index_get(int index, xl_light_index *out)
{
	if (!out || index < 0 || index >= xl_light_index_count()) return 0;
	const auto &row = authored->light_indices[index];
	*out = { row[0], row[1], row[2], row[3] };
	return 1;
}

extern "C" int xl_light_delta_get(int index, xl_light_delta *out)
{
	if (!out || index < 0 || index >= xl_light_delta_count()) return 0;
	const auto &row = authored->delta_lights[index];
	*out = { row[0], row[1], { row[3], row[4], row[5], row[6] } };
	return 1;
}

extern "C" int xl_changing_light(int segment, int side, int direction)
{
	if (!authored || !xl_level_extensions_active()) return 0;
	if (segment < 0 || segment >= Num_segments || side < 0 || side >= 6) return 1;
	const auto first = std::lower_bound(ordered.begin(), ordered.end(), std::pair<int, int>{ segment, side },
	                                    [&](std::size_t index, const std::pair<int, int> &key) {
		                                    const auto &row = authored->light_indices[index];
		                                    return row[0] < key.first || (row[0] == key.first && row[1] < key.second);
	                                    });
	for (auto at = first; at != ordered.end(); ++at) {
		const auto &index = authored->light_indices[*at];
		if (index[0] != segment || index[1] != side) break;
		for (int i = 0; i < index[2]; ++i) {
			const auto &delta = authored->delta_lights[index[3] + i];
			auto &face = Segments[delta[0]].sides[delta[1]];
			for (int corner = 0; corner < 4; ++corner) {
				const auto value = static_cast<int64_t>(face.uvls[corner].l) + static_cast<int64_t>(direction) * delta[corner + 3] * DL_SCALE;
				// Preserve per-contribution lower clamping and define source signed overflow
				face.uvls[corner].l = static_cast<fix>((std::max) (INT64_C(0), (std::min) (value, static_cast<int64_t>(INT32_MAX))));
			}
		}
	}
	segment_light(segment, side, direction);
	return 1;
}
