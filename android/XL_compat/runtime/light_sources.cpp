#include "light_sources.h"
#include "flicker.h"
#include "preview.h"
#include "skybox_render.h"
#include <stdexcept>
#include <algorithm>
#include <tuple>
#include <cmath>
extern "C" {
#include "gameseg.h"
#include "mission.h"
#include "wall.h"
#include "switch.h"
#include "bm.h"
#include "effects.h"
#include "game.h"
#include "d1_in_d2/d1_in_d2_levels.h"
}

namespace xl_compat
{
namespace
{
bool trigger_target(int segment, int side)
{
	for (int i = 0; i < Num_triggers; ++i)
		for (int link = 0; link < Triggers[i].num_links; ++link)
			if (Triggers[i].seg[link] == segment && Triggers[i].side[link] == side) return true;
	return false;
}

int xl_trigger_type(const trigger &entry)
{
	short flags;
	// XL loads native D1 records as TT_DESCENT1, not their D2 adapter type
	return d1_in_d2_trigger_source_flags(&entry, &flags) ? -1 : entry.type;
}

bool triggered_light(int segment, int side, bool opposite = false)
{
	const auto &surface = Segments[segment].sides[side];
	const bool force_field = (TmapInfo[surface.tmap_num].flags & TMI_FORCE_FIELD) != 0;
	for (int i = 0; i < Num_triggers; ++i) {
		const int type = xl_trigger_type(Triggers[i]);
		if (type != TT_OPEN_WALL && type != TT_CLOSE_WALL && (force_field || (type != TT_LIGHT_OFF && type != TT_LIGHT_ON))) continue;
		for (int link = 0; link < Triggers[i].num_links; ++link)
			if (Triggers[i].seg[link] == segment && Triggers[i].side[link] == side) return true;
	}
	if (opposite || !force_field) return false;
	const int child = Segments[segment].children[side];
	if (child < 0 || child > Highest_segment_index) return false;
	const int connected = find_connect_side(&Segments[segment], &Segments[child]);
	return connected >= 0 && triggered_light(child, connected, true);
}

bool volatile_wall(int segment, int side)
{
	const auto wall_index = Segments[segment].sides[side].wall_num;
	if (wall_index < 0 || wall_index >= Num_walls) return false;
	const auto &wall = Walls[wall_index];
	if (wall.type == WALL_OPEN || wall.type == WALL_DOOR || wall.type == WALL_BLASTABLE || (wall.type == WALL_CLOAKED && wall.cloak_value < 31)) return true;
	for (int i = 0; i < Num_triggers; ++i) {
		const int type = xl_trigger_type(Triggers[i]);
		if (type != TT_OPEN_DOOR && type != TT_ILLUSION_OFF && type != TT_OPEN_WALL) continue;
		for (int link = 0; link < Triggers[i].num_links; ++link)
			// Preserve the pinned CWall::IsVolatile predicate: either coordinate matches
			if (Triggers[i].seg[link] == segment || Triggers[i].side[link] == side) return true;
	}
	return false;
}

bool destructible_texture(int texture)
{
	if (!texture) return false;
	const auto &info = TmapInfo[texture];
	if (info.eclip_num == -1) return info.destroyed != -1;
	if (info.eclip_num < 0) return false;
	if (info.eclip_num >= Num_effects) throw std::runtime_error("Invalid geometry light effect clip");
	const auto &effect = Effects[info.eclip_num];
	return effect.dest_bm_num != -1 && !(effect.flags & EF_ONE_SHOT);
}

bool textured_face(int segment, int side)
{
	const auto &seg = Segments[segment];
	const auto wall_index = seg.sides[side].wall_num;
	if (wall_index < 0 || wall_index >= Num_walls) return seg.children[side] == -1;
	const auto &wall = Walls[wall_index];
	// Pinned XL FADE_LEVELS is 31, independent of the engine's palette fade table
	if (wall.type != WALL_OPEN && (wall.type != WALL_CLOAKED || wall.cloak_value < 31)) return true;
	if (trigger_target(segment, side)) return true;
	const auto child = seg.children[side];
	if (child < 0 || child > Highest_segment_index) return true;
	const auto opposite = find_connect_side(&Segments[segment], &Segments[child]);
	if (opposite < 0) return true;
	const auto opposite_wall = Segments[child].sides[opposite].wall_num;
	return opposite_wall < 0 || opposite_wall >= Num_walls || trigger_target(child, opposite);
}
} // namespace

std::vector<int> classic_lighting_face_vertices(int segment)
{
	if (!geometry_lighting_available() || segment < 0 || segment > Highest_segment_index)
		throw std::runtime_error("Invalid classic lighting mesh segment");
	std::vector<int> vertices;
	for (int side = 0; side < 6; ++side)
		if (textured_face(segment, side))
			for (int corner = 0; corner < 4; ++corner) {
				const int vertex = Segments[segment].verts[Side_to_verts_int[side][corner]];
				if (vertex < 0 || vertex > Highest_vertex_index) throw std::runtime_error("Invalid classic mesh face vertex");
				vertices.push_back(vertex);
			}
	std::sort(vertices.begin(), vertices.end());
	vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
	return vertices;
}

std::vector<GeometryLightDistance> classic_nearest_vertex_lights(int vertex,
                                                                 const std::vector<ClassicGeometryLight> &sources, const std::vector<std::vector<bool>> &visibility)
{
	if (!geometry_lighting_available() || vertex < 0 || vertex > Highest_vertex_index ||
	    visibility.size() != static_cast<std::size_t>(Highest_segment_index) + 1)
		throw std::runtime_error("Invalid classic nearest-light vertex or visibility table");
	const auto &position = Vertices[vertex];
	const FixedLightPoint point{ position.x, position.y, position.z };
	std::vector<GeometryLightDistance> candidates;
	for (std::size_t index = 0; index < sources.size(); ++index) {
		const auto &source = sources[index];
		if (source.segment < 0 || source.segment > Highest_segment_index || source.side < 0 || source.side >= 6 ||
		    visibility[source.segment].size() != static_cast<std::size_t>(Highest_vertex_index) + 1)
			throw std::runtime_error("Invalid classic nearest-light source or visibility row");
		bool own_vertex = false;
		for (int corner = 0; corner < 4; ++corner)
			if (Segments[source.segment].verts[Side_to_verts_int[source.side][corner]] == vertex) own_vertex = true;
		const bool visible = visibility[source.segment][vertex];
		if (!own_vertex && !visible) continue;
		const auto distance = geometry_vertex_candidate(own_vertex ? 0 : geometry_fixed_distance(point, source.geometry.center), source.radius, own_vertex, visible);
		if (distance) candidates.push_back({ static_cast<int>(index), *distance });
	}
	return nearest_geometry_lights(std::move(candidates));
}

std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureColorTable &colors)
{
	TextureRgbTable rgb{};
	for (std::size_t i = 0; i < colors.size(); ++i)
		for (unsigned channel = 0; channel < 3; ++channel)
			rgb[i][channel] = static_cast<float>(colors[i].channels[channel]) / static_cast<float>(0x7fffffff);
	return compile_classic_geometry_lights(brightness, rgb);
}

std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureRgbTable &colors)
{
	if (!geometry_lighting_available())
		throw std::runtime_error("Geometry light materials require D1 or admitted XL geometry");
	if (Game_mode & GM_MULTI) throw std::runtime_error("Classic geometry light profile requires single-player mode");
	ClassicFaceLightMaterials materials(brightness, colors);
	std::vector<ClassicGeometryLight> result;
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		for (int side = 0; side < 6; ++side) {
			if (xl_skybox_segment(segment)) continue;
			if (!textured_face(segment, side)) continue;
			const auto &surface = Segments[segment].sides[side];
			const auto material = materials.face(surface.tmap_num, surface.tmap_num2 & 0x3fff, xl_preview_outdoors_segment(segment) != 0);
			if (material) {
				auto geometry = compile_classic_lighting_side(segment, side);
				const auto direction = geometry_emission_direction(geometry.fixed_normals);
				result.push_back({ segment, side, geometry, *material, direction });
				auto &light = result.back();
				light.radius = (geometry.outer_radius / 65536.0f) / 2.0f;
				light.destructible = destructible_texture(material->created_texture);
				light.triggered = triggered_light(segment, side);
				light.volatile_wall = volatile_wall(segment, side);
				light.flickering = xl_flicker_has_source(segment, side) != 0;
				for (int i = 0; i < Num_flickering_lights; ++i)
					if (Flickering_lights[i].segnum == segment && Flickering_lights[i].sidenum == side) light.flickering = true;
			}
		}
	std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
		return std::make_tuple(a.variable(), a.segment, a.side) < std::make_tuple(b.variable(), b.segment, b.side);
	});
	return result;
}
std::vector<std::vector<bool>> compile_classic_vertex_visibility(const std::vector<ClassicGeometryLight> &sources,
                                                                 const ClassicVisibilityCamera &optics)
{
	const auto graph = classic_light_route_graph();
	const auto segment_visibility = classic_segment_visibility(optics);
	std::vector<std::vector<int>> mesh_vertices;
	for (int segment = 0; segment <= Highest_segment_index; ++segment) mesh_vertices.push_back(classic_lighting_face_vertices(segment));
	std::vector<std::vector<bool>> result(graph.centers.size(), std::vector<bool>(static_cast<std::size_t>(Highest_vertex_index) + 1, false));
	std::vector<std::optional<ClassicDistanceRow>> rows(graph.centers.size());
	for (const auto &source : sources) {
		if (source.segment < 0 || static_cast<std::size_t>(source.segment) >= rows.size() || source.side < 0 || source.side >= 6)
			throw std::runtime_error("Invalid source for classic vertex visibility");
		auto &row = rows[source.segment];
		if (!row) row = classic_distance_row(graph, segment_visibility, source.segment);
		const auto views = classic_light_view_candidates(source.segment, source.side, optics);
		for (const auto destination : views.segments) {
			if (classic_segment_distance(*row, graph.centers, destination, {}, {}) < 0) continue;
			for (const auto vertex : mesh_vertices[destination]) result[source.segment][vertex] = true;
		}
	}
	return result;
}

ClassicAmbientLighting::ClassicAmbientLighting(const std::vector<ClassicGeometryLight> &sources, const ClassicVisibilityCamera &optics)
    : ClassicAmbientLighting(sources, compile_classic_vertex_visibility(sources, optics))
{
}

ClassicAmbientLighting::ClassicAmbientLighting(const std::vector<ClassicGeometryLight> &sources, const std::vector<std::vector<bool>> &visibility)
{
	if (!geometry_lighting_available() || Highest_vertex_index < 0)
		throw std::runtime_error("Ambient snapshot requires loaded D1 or admitted XL geometry");
	if (Game_mode & GM_MULTI) throw std::runtime_error("Classic ambient profile requires single-player mode");
	std::vector<std::int32_t> radii;
	for (int segment = 0; segment <= Highest_segment_index; ++segment) radii.push_back(classic_segment_average_radius(segment));
	normals_ = compile_classic_vertex_normals();
	for (int vertex = 0; vertex <= Highest_vertex_index; ++vertex) {
		const auto &v = Vertices[vertex];
		positions_.push_back({ v.x, v.y, v.z });
		nearest_.push_back(classic_nearest_vertex_lights(vertex, sources, visibility));
	}
	for (const auto &light : sources) {
		Source source;
		source.light = light;
		source.segment_radius = radii.at(light.segment);
		for (int corner = 0; corner < 4; ++corner)
			source.vertices[corner] = Segments[light.segment].verts[Side_to_verts_int[light.side][corner]];
		sources_.push_back(source);
	}
}

void ClassicAmbientLighting::set_source_state(std::size_t index, bool enabled, std::int32_t stored_distance)
{
	auto &source = sources_.at(index);
	source.enabled = enabled;
	source.stored_distance = stored_distance;
}

std::vector<Rgb> ClassicAmbientLighting::gather(const ClassicAmbientProfile &profile) const
{
	for (const float value : { profile.range_scale, profile.linear_attenuation, profile.quadratic_attenuation, profile.cutoff, profile.brightness })
		if (!std::isfinite(value) || value < 0) throw std::runtime_error("Invalid ambient lighting profile");
	for (const auto &color : { profile.material_ambient, profile.material_diffuse })
		for (const auto value : color)
			if (!std::isfinite(value) || value < 0) throw std::runtime_error("Invalid ambient lighting material");
	std::vector<Rgb> colors;
	colors.reserve(positions_.size());
	for (std::size_t vertex = 0; vertex < positions_.size(); ++vertex) {
		std::vector<GeometryLightDistance> candidates;
		for (const auto &entry : nearest_[vertex]) {
			const auto &source = sources_[entry.index];
			if (!source.enabled) continue;
			const auto distance = geometry_ambient_distance(geometry_fixed_distance(positions_[vertex], source.light.geometry.center), source.light.radius, source.segment_radius);
			if (distance) candidates.push_back({ entry.index, source.stored_distance });
		}
		GeometryColorSum sum;
		for (const auto &entry : geometry_active_lights(candidates)) {
			const auto &source = sources_[entry.index];
			const auto &light = source.light;
			if (light.material.rgb == Rgb{}) continue;
			if (light.material.ambient && std::find(source.vertices.begin(), source.vertices.end(), static_cast<int>(vertex)) == source.vertices.end()) continue;
			GeometryColorInput input;
			input.face = light.geometry.face;
			for (unsigned axis = 0; axis < 3; ++axis) {
				input.center[axis] = light.geometry.center[axis] / 65536.0f;
				input.receiver[axis] = positions_[vertex][axis] / 65536.0f;
			}
			input.emission_direction = light.emission_direction;
			input.normal = normals_[vertex].normal;
			if (profile.renormalize_normals) {
				float length = 0;
				for (const auto axis : input.normal) length += axis * axis;
				length = std::sqrt(length);
				if (length)
					for (auto &axis : input.normal) axis /= length;
			}
			input.light_color = light.material.rgb;
			input.material_ambient = profile.material_ambient;
			input.material_diffuse = profile.material_diffuse;
			input.attenuation = { 0, 0, 0, profile.range_scale, profile.linear_attenuation, profile.quadratic_attenuation, profile.cutoff, light.material.brightness };
			input.self = light.material.ambient;
			sum.add(geometry_source_color(input), input.self);
		}
		auto color = sum.finish();
		for (auto &channel : color) channel = (std::min) (1.0f, channel * profile.brightness);
		colors.push_back(color);
	}
	return colors;
}

void ClassicAmbientLighting::clear()
{
	sources_ = {};
	positions_ = {};
	normals_ = {};
	nearest_ = {};
}
} // namespace xl_compat
