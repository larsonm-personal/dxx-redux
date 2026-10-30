#include "audio_geometry.h"
#include "preview.h"
#include <stdexcept>
namespace xl_compat
{
ClassicAudioSpatial::ClassicAudioSpatial()
{
	if (!xl_level_extensions_active()) throw std::runtime_error("XL spatial audio requires an active XL level");
	graph = classic_light_route_graph();
	visibility = classic_segment_visibility(classic_default_lighting_camera());
	routes.resize(graph.centers.size());
}
AudioSpatialOutput ClassicAudioSpatial::output(int listener_segment, int sound_segment, const FixedLightPoint &listener, const FixedLightPoint &sound, const FixedLightPoint &right, const FixedLightPoint &up, int volume, int range, int decay, bool reverse, int hires)
{
	if (listener_segment < 0 || sound_segment < 0 || std::size_t(listener_segment) >= graph.centers.size() || std::size_t(sound_segment) >= graph.centers.size()) throw std::runtime_error("Invalid XL spatial sound segment");
	const auto direct = audio_point_distance(listener, sound);
	if (direct > audio_extended_range(range, decay)) return audio_spatial_output(-1, volume, range, decay, 0, reverse, hires);
	if (listener_segment != sound_segment && route_listener != listener_segment) {
		// The source caches all destinations until the listener changes segment
		// At that point routing observes the current wall passability
		auto current = classic_light_route_graph();
		if (current.centers != graph.centers) throw std::runtime_error("XL spatial audio belongs to another level");
		std::vector<std::array<GeometryRouteEdge, 6>> edges(current.edges.size());
		for (std::size_t i = 0; i < edges.size(); ++i)
			for (unsigned side = 0; side < 6; ++side)
				if (current.edges[i][side].passable) edges[i][side] = { current.edges[i][side].child, current.edges[i][side].cost };
		auto updated = geometry_weighted_routes(edges, listener_segment);
		graph = std::move(current);
		routes = std::move(updated);
		route_listener = listener_segment;
	}
	const auto distance = audio_geometry_distance(graph, visibility, routes, listener_segment, sound_segment, listener, sound, range, decay);
	const auto angle = distance < 0 ? std::int16_t(0) : audio_ear_angle(right, up, listener, sound);
	return audio_spatial_output(distance, volume, range, decay, angle, reverse, hires);
}
} // namespace xl_compat
