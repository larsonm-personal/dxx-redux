#include "lighting.h"
#include "inventory.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
std::int32_t geometry_default_magnitude(const FixedLightPoint &vector)
{
	const double x = vector[0], y = vector[1], z = vector[2];
	const double magnitude = std::sqrt(x * x + y * y + z * z);
	if (magnitude > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL default magnitude exceeds fixed-point range");
	return static_cast<std::int32_t>(magnitude);
}

FixedLightPoint geometry_default_direction(const FixedLightPoint &vector)
{
	const auto magnitude = geometry_default_magnitude(vector);
	FixedLightPoint result{};
	if (magnitude)
		for (unsigned axis = 0; axis < 3; ++axis)
			result[axis] = static_cast<std::int32_t>(double(vector[axis]) / magnitude * 65536.0);
	return result;
}

namespace
{
LightPoint difference(const LightPoint &a, const LightPoint &b)
{
	return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}
float dot(const LightPoint &a, const LightPoint &b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
LightPoint along(const LightPoint &origin, const LightPoint &direction, float distance)
{
	return { origin[0] + direction[0] * distance, origin[1] + direction[1] * distance, origin[2] + direction[2] * distance };
}
void sort_geometry_distances(std::vector<GeometryLightDistance> &distances, int left, int right)
{
	int l = left, r = right;
	const auto pivot = distances[(l + r) / 2].distance;
	do {
		while (distances[l].distance < pivot) ++l;
		while (distances[r].distance > pivot) --r;
		if (l <= r) {
			if (l < r) std::swap(distances[l], distances[r]);
			++l;
			--r;
		}
	} while (l <= r);
	if (l < right) sort_geometry_distances(distances, l, right);
	if (left < r) sort_geometry_distances(distances, left, r);
}
} // namespace

std::vector<GeometryRouteNode> geometry_weighted_routes(const std::vector<std::array<GeometryRouteEdge, 6>> &graph, int start)
{
	if (graph.size() > 32767 || start < 0 || static_cast<std::size_t>(start) >= graph.size())
		throw std::runtime_error("Invalid geometry route graph");
	for (const auto &segment : graph)
		for (const auto &edge : segment)
			if (edge.child >= 0 && (static_cast<std::size_t>(edge.child) >= graph.size() || !edge.cost))
				throw std::runtime_error("Invalid geometry route edge");
	std::vector<GeometryRouteNode> result(graph.size());
	std::vector<int> heads(65536, -1), links(graph.size(), -1);
	std::vector<bool> settled(graph.size(), false);
	result[start].cost = 0;
	heads[0] = start;
	std::size_t queued = 1;
	std::uint16_t bucket = 0;
	while (queued) {
		while (heads[bucket] < 0) ++bucket;
		const int node = heads[bucket];
		heads[bucket] = links[node];
		--queued;
		settled[node] = true;
		for (unsigned side = 0; side < 6; ++side) {
			const auto &edge = graph[node][side];
			if (edge.child < 0) continue;
			const auto cost = std::int64_t(result[node].cost) + edge.cost;
			if (cost >= std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("Geometry route cost overflow");
			auto &target = result[edge.child];
			if (target.cost >= 0 && cost >= target.cost) continue;
			if (settled[edge.child]) throw std::runtime_error("Geometry Dial queue ordering failed");
			if (target.cost >= 0) {
				const auto old_bucket = static_cast<std::uint16_t>(target.cost);
				int previous = -1, current = heads[old_bucket];
				while (current >= 0 && current != edge.child) {
					previous = current;
					current = links[current];
				}
				if (current < 0) throw std::runtime_error("Missing geometry Dial queue entry");
				if (previous < 0) heads[old_bucket] = links[current];
				else links[previous] = links[current];
			} else ++queued;
			const auto next_bucket = static_cast<std::uint16_t>(cost);
			links[edge.child] = heads[next_bucket];
			heads[next_bucket] = edge.child;
			target = { static_cast<std::int32_t>(cost), node, static_cast<int>(side) };
		}
	}
	return result;
}

std::vector<int> geometry_route_path(const std::vector<GeometryRouteNode> &routes, int destination)
{
	if (destination < 0 || static_cast<std::size_t>(destination) >= routes.size()) throw std::runtime_error("Invalid route destination");
	if (routes[destination].cost < 0) return {};
	std::vector<int> path;
	for (int node = destination; node >= 0; node = routes[node].predecessor) {
		if (static_cast<std::size_t>(node) >= routes.size() || path.size() >= routes.size()) throw std::runtime_error("Invalid route predecessor chain");
		const auto &entry = routes[node];
		if (entry.cost < 0 || (!path.empty() && entry.cost >= routes[path.back()].cost)) throw std::runtime_error("Route costs do not decrease toward source");
		if (entry.predecessor < 0 && entry.cost != 0) throw std::runtime_error("Route root has nonzero cost");
		path.push_back(node);
	}
	std::reverse(path.begin(), path.end());
	return path;
}

GeometryRouteInterior geometry_shorten_route(const std::vector<int> &path,
                                             const std::function<bool(int, int)> &visible, const std::function<std::int32_t(int, int)> &distance)
{
	if (path.empty()) return {};
	if (!visible || !distance || std::any_of(path.begin(), path.end(), [](int node) { return node < 0; }))
		throw std::runtime_error("Invalid route shortening input");
	GeometryRouteInterior result{ 0, -1, -1 };
	if (path.size() < 3) return result;
	const auto last = path.size() - 2;
	result.first = path[1];
	result.last = path[last];
	for (std::size_t i = 1, j; i < last; i = j) {
		std::size_t endpoint = i;
		for (j = i + 1; j <= last; ++j) {
			if (j > i + 1 && !visible(path[i], path[j])) {
				--j;
				break;
			}
			endpoint = j;
		}
		const auto leg = distance(path[i], path[endpoint]);
		const auto sum = std::int64_t(result.distance) + leg;
		if (leg < 0 || sum > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("Invalid shortened route distance");
		result.distance = static_cast<std::int32_t>(sum);
	}
	return result;
}

float geometry_route_distance_scale(std::int32_t maximum)
{
	if (maximum < 0) throw std::runtime_error("Invalid maximum route distance");
	return maximum > 65534 ? static_cast<float>(maximum) / 65534.0f : 1.0f;
}

std::uint16_t geometry_pack_route_distance(std::int32_t distance, float scale)
{
	if (!std::isfinite(scale) || scale < 1) throw std::runtime_error("Invalid route distance scale");
	if (distance < 0) return 65535;
	const float packed = static_cast<float>(distance) / scale;
	if (!std::isfinite(packed) || packed >= 65535)
		throw std::runtime_error("Route distance collides with unreachable sentinel");
	return static_cast<std::uint16_t>(packed);
}

std::int32_t geometry_unpack_route_distance(std::uint16_t distance, float scale)
{
	if (!std::isfinite(scale) || scale < 1) throw std::runtime_error("Invalid route distance scale");
	if (distance == 65535) return -1;
	const float unpacked = static_cast<float>(distance) * scale;
	if (!std::isfinite(unpacked) || double(unpacked) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("Decoded route distance exceeds fixed-point range");
	return static_cast<std::int32_t>(unpacked);
}

GeometryScreenPoint geometry_screen_point(const std::array<float, 2> &xy, float depth, float half_width, float half_height)
{
	if (!std::isfinite(depth) || !std::isfinite(xy[0]) || !std::isfinite(xy[1]) || !std::isfinite(half_width) || !std::isfinite(half_height) || half_width <= 0 || half_height <= 0)
		throw std::runtime_error("Unsupported geometry visibility projection");
	const auto coordinate = [depth](float value, float half_extent) {
		// Defined emulation of measured MSVC x86/x64 integer-indefinite output
		if (depth == 0) return std::numeric_limits<std::int32_t>::min();
		const float screen = half_extent * (1.0f + value / std::abs(depth));
		if (!std::isfinite(screen) || double(screen) < std::numeric_limits<std::int32_t>::min() || double(screen) > std::numeric_limits<std::int32_t>::max())
			return std::numeric_limits<std::int32_t>::min();
		return static_cast<std::int32_t>(screen);
	};
	// ProjectAndEncode overwrites Encode's screen-edge flags with only CC_BEHIND
	return { coordinate(xy[0], half_width), coordinate(xy[1], half_height), depth < 0 };
}

std::array<FixedLightPoint, 3> geometry_scaled_camera(const std::array<FixedLightPoint, 3> &basis,
                                                      const FixedLightPoint &aspect, std::int32_t zoom)
{
	if (zoom <= 0 || aspect[0] <= 0 || aspect[1] <= 0 || aspect[2] <= 0)
		throw std::runtime_error("Invalid geometry camera scale");
	const auto multiply = [](std::int32_t a, std::int32_t b) {
		const double value = double(b) / 65536.0 * double(a);
		if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("Geometry camera scale exceeds fixed-point range");
		return static_cast<std::int32_t>(value);
	};
	auto scale = aspect;
	if (zoom < 65536) scale[2] = multiply(scale[2], zoom);
	else if (zoom > 65536) {
		const auto reciprocal = static_cast<std::int32_t>(65536.0 / double(zoom) * 65536.0);
		scale[0] = multiply(scale[0], reciprocal);
		scale[1] = multiply(scale[1], reciprocal);
	}
	auto result = basis;
	for (unsigned row = 0; row < 3; ++row)
		for (auto &component : result[row]) component = multiply(component, scale[row]);
	return result;
}

LightPoint geometry_camera_view(const FixedLightPoint &world, const FixedLightPoint &position,
                                const std::array<FixedLightPoint, 3> &scaled_basis)
{
	LightPoint delta, result;
	for (unsigned axis = 0; axis < 3; ++axis)
		delta[axis] = static_cast<float>(world[axis]) / 65536.0f - static_cast<float>(position[axis]) / 65536.0f;
	for (unsigned row = 0; row < 3; ++row) {
		LightPoint basis;
		for (unsigned axis = 0; axis < 3; ++axis) basis[axis] = static_cast<float>(scaled_basis[row][axis]) / 65536.0f;
		result[row] = dot(delta, basis);
	}
	return result;
}

std::optional<GeometryPortal> geometry_light_portal(const std::array<GeometryScreenPoint, 4> &points,
                                                    const GeometryPortal &current, int width, int height)
{
	if (width <= 0 || height <= 0 || current.left < 0 || current.top < 0 || current.right >= width || current.bottom >= height || current.left > current.right || current.top > current.bottom)
		throw std::runtime_error("Invalid geometry visibility viewport or portal");
	// Preserve the reference's finite initial bounds, including large coordinates
	GeometryPortal face{ 32767, -32767, 32767, -32767 };
	bool any_behind = false, all_behind = true;
	for (const auto &point : points) {
		face.left = std::min(face.left, point.x);
		face.right = std::max(face.right, point.x);
		face.top = std::min(face.top, point.y);
		face.bottom = std::max(face.bottom, point.y);
		any_behind = any_behind || point.behind;
		all_behind = all_behind && point.behind;
	}
	if (all_behind || face.right < 0 || face.bottom < 0 || face.left >= width || face.top >= height) return std::nullopt;
	face.left = std::max(face.left, 0);
	face.right = std::min(face.right, width - 1);
	face.top = std::max(face.top, 0);
	face.bottom = std::min(face.bottom, height - 1);
	if (any_behind) return current;
	// CodePortal uses strict comparisons: edge-touching rectangles survive
	if (face.right < current.left || face.left > current.right || face.bottom < current.top || face.top > current.bottom) return std::nullopt;
	return GeometryPortal{ std::max(current.left, face.left), std::min(current.right, face.right), std::max(current.top, face.top), std::min(current.bottom, face.bottom) };
}

std::vector<GeometryPortalVisit> geometry_portal_traversal(const std::vector<GeometryPortalSegment> &segments,
                                                           int start, int width, int height, unsigned pass_limit)
{
	return geometry_portal_traversal(segments.size(), [&segments](int index) { return segments[index]; }, start, width, height, pass_limit);
}

std::vector<GeometryPortalVisit> geometry_portal_traversal(std::size_t segment_count,
                                                           const std::function<GeometryPortalSegment(int)> &segment, int start, int width, int height, unsigned pass_limit)
{
	if (start < 0 || static_cast<std::size_t>(start) >= segment_count || width <= 0 || height <= 0 || pass_limit > segment_count || !segment)
		throw std::runtime_error("Invalid geometry portal traversal input");
	std::vector<GeometryPortalVisit> visits;
	visits.reserve(segment_count); // Keep current portal references stable while appending
	std::vector<std::size_t> positions(segment_count, segment_count);
	std::vector<bool> processed(segment_count, false);
	visits.push_back({ start, { 0, width - 1, 0, height - 1 } });
	positions[start] = 0;
	for (unsigned pass = 0; pass < pass_limit; ++pass) {
		const auto tail = visits.size();
		for (std::size_t head = 0; head < tail; ++head) {
			if (processed[head]) continue;
			processed[head] = true;
			const auto &current = visits[head];
			for (const auto &side : segment(current.segment)) {
				if (side.child < 0) continue;
				if (static_cast<std::size_t>(side.child) >= segment_count)
					throw std::runtime_error("Invalid geometry traversal child");
				const auto portal = geometry_light_portal(side.points, current.portal, width, height);
				if (!portal) continue;
				const auto position = positions[side.child];
				if (position == segment_count) {
					positions[side.child] = visits.size();
					visits.push_back({ side.child, *portal });
				} else {
					auto &old = visits[position].portal;
					const bool expands = portal->left < old.left || portal->right > old.right || portal->top < old.top || portal->bottom > old.bottom;
					if (expands) {
						old = { std::min(old.left, portal->left), std::max(old.right, portal->right), std::min(old.top, portal->top), std::max(old.bottom, portal->bottom) };
						processed[position] = false;
					}
				}
			}
		}
	}
	return visits;
}

LightPoint geometry_emission_direction(const std::array<FixedLightPoint, 2> &normals)
{
	LightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto average = (std::int64_t(normals[0][axis]) + normals[1][axis]) / 2;
		result[axis] = static_cast<float>(average) / 65536.0f;
	}
	const float magnitude = std::sqrt(dot(result, result));
	if (magnitude)
		for (auto &value : result) value /= magnitude;
	return result;
}

std::optional<std::int32_t> geometry_vertex_candidate(std::int32_t center_distance, float radius, bool own_vertex, bool visible)
{
	if (own_vertex) return 0;
	if (!visible) return std::nullopt;
	const float offset = radius * 6553.6f;
	if (center_distance < 0 || !std::isfinite(offset) || offset < 0 || double(offset) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("Invalid geometry light radius or distance");
	const auto distance = center_distance - static_cast<std::int32_t>(offset);
	if (distance > 222 * 65536) return std::nullopt;
	return distance;
}

bool classic_light_texture_transparent(unsigned base_flags, unsigned overlay_flags, bool has_overlay)
{
	if (has_overlay) {
		if (overlay_flags & 2) return true;
		if (!(overlay_flags & 1)) return false;
	}
	return (base_flags & 3) != 0;
}

FixedLightPoint geometry_fixed_cross(const FixedLightPoint &a, const FixedLightPoint &b)
{
	for (unsigned axis = 0; axis < 3; ++axis)
		if (a[axis] < -262144 || a[axis] > 262143 || b[axis] < -262144 || b[axis] > 262143)
			throw std::runtime_error("XL fixed cross requires scaled edges");
	FixedLightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto j = (axis + 1) % 3, k = (axis + 2) % 3;
		const auto product = std::int64_t(a[j]) * b[k] - std::int64_t(a[k]) * b[j];
		result[axis] = static_cast<std::int32_t>(product / 65536 - (product < 0 && product % 65536 ? 1 : 0));
	}
	return result;
}

FixedLightPoint geometry_side_center(const std::array<LightPoint, 4> &corners, const std::array<FixedLightPoint, 2> &normals, const FixedLightPoint &plane_point)
{
	const auto checked = [](double value) -> std::int32_t {
		if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("Geometry side center exceeds fixed-point range");
		return static_cast<std::int32_t>(value);
	};
	FixedLightPoint center{}, start{}, direction{}, offset{};
	for (unsigned axis = 0; axis < 3; ++axis) {
		float sum = 0;
		for (const auto &corner : corners) sum += corner[axis];
		center[axis] = checked((sum / 4.0f) * 65536.0f);
		const auto average = (std::int64_t(normals[0][axis]) + normals[1][axis]) / 2;
		start[axis] = checked(double(center[axis]) + average);
		direction[axis] = checked(-2.0 * average);
		// Reference intersects against the original fixed vertex, not its float copy
		offset[axis] = checked(double(start[axis]) - plane_point[axis]);
	}
	const auto fixed_dot = [&](const FixedLightPoint &a, const FixedLightPoint &b) {
		return checked((double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2]) / 65536.0);
	};
	const auto denominator = checked(-double(fixed_dot(normals[0], direction)));
	if (!denominator) return center;
	const auto numerator = fixed_dot(normals[0], offset);
	const auto fraction = checked(double(numerator) / denominator * 65536.0);
	if (fraction > 65536) return center;
	for (unsigned axis = 0; axis < 3; ++axis) {
		// XL's active FixMul macro truncates the double product toward zero
		const auto scaled = checked(double(fraction) / 65536.0 * direction[axis]);
		center[axis] = checked(double(start[axis]) + scaled);
	}
	return center;
}

std::array<LightPoint, 3> geometry_orient_normals(const std::array<LightPoint, 2> &normals,
                                                  const std::array<FixedLightPoint, 2> &fixed_normals, const LightPoint &float_direction, const FixedLightPoint &fixed_direction)
{
	std::array<LightPoint, 3> result{ normals[0], normals[1], {} };
	for (unsigned i = 0; i < 2; ++i)
		if (dot(result[i], float_direction) < 0)
			for (auto &value : result[i]) value = -value;
	for (unsigned axis = 0; axis < 3; ++axis) result[2][axis] = (result[0][axis] + result[1][axis]) / 2;
	for (unsigned i = 0; i < 2; ++i) {
		const auto &n = fixed_normals[i];
		const double projection = (double(n[0]) * fixed_direction[0] + double(n[1]) * fixed_direction[1] + double(n[2]) * fixed_direction[2]) / 65536.0;
		// Fixed dot truncates toward zero: a negative fraction does not trigger a flip
		if (std::trunc(projection) < 0)
			for (auto &value : result[i]) value = -value;
	}
	// Pinned XL flips float normals again in its fixed pass and retains the earlier average
	return result;
}

LightPoint geometry_triangle_normal(const LightPoint &a, const LightPoint &b, const LightPoint &c)
{
	const auto u = difference(b, a), v = difference(c, b);
	LightPoint normal{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
	const float magnitude = std::sqrt(dot(normal, normal));
	if (magnitude)
		for (auto &coordinate : normal) coordinate /= magnitude;
	return normal;
}

LightPoint geometry_portal_normal(const std::array<LightPoint, 3> &points, const std::array<std::uint16_t, 3> &vertices)
{
	for (const auto vertex : vertices)
		if (vertex == 0xffff) throw std::runtime_error("Invalid portal normal vertex");
	std::array<unsigned, 3> order{ 0, 1, 2 };
	for (unsigned i = 1; i < 3; ++i)
		for (unsigned j = 0; j < i; ++j)
			if (vertices[order[j]] > vertices[order[i]]) std::swap(order[i], order[j]);
	auto normal = geometry_triangle_normal(points[order[0]], points[order[1]], points[order[2]]);
	// Fourth reference vertex is the 0xffff sentinel, which sorts last
	if ((order[0] + 3) % 4 == order[1] || (order[1] + 3) % 4 == order[2])
		for (auto &coordinate : normal) coordinate = -coordinate;
	return normal;
}

LightPoint geometry_light_point(const GeometryLightFace &face, const LightPoint &receiver)
{
	if (face.corner_count < 3 || face.corner_count > 4 || face.triangle_count < 1 || face.triangle_count > 2)
		throw std::runtime_error("Invalid geometry light face dimensions");
	for (unsigned i = 0; i < face.triangle_count; ++i) {
		const auto &indices = face.triangles[i];
		for (const auto index : indices)
			if (index >= face.corner_count) throw std::runtime_error("Invalid geometry light triangle corner");
	}
	for (unsigned i = 0; i < face.triangle_count; ++i) {
		const auto &normal = face.normals[i];
		if (std::sqrt(dot(normal, normal)) < 1e-6f) continue;
		const auto &indices = face.triangles[i];
		const auto &a = face.corners[indices[0]];
		const float distance = dot(difference(receiver, a), normal);
		const auto projected = along(receiver, normal, -distance);
		const auto v0 = difference(face.corners[indices[2]], a);
		const auto v1 = difference(face.corners[indices[1]], a);
		const auto v2 = difference(projected, a);
		const float dot00 = dot(v0, v0), dot01 = dot(v0, v1), dot02 = dot(v0, v2), dot11 = dot(v1, v1), dot12 = dot(v1, v2);
		const float inverse = 1.0f / (dot00 * dot11 - dot01 * dot01);
		const float u = (dot11 * dot02 - dot01 * dot12) * inverse;
		const float v = (dot00 * dot12 - dot01 * dot02) * inverse;
		if (!(u < -0.00001f || v < -0.00001f || u + v > 1.00001f))
			return std::abs(distance) < 0.001f ? receiver : projected;
	}
	LightPoint result{};
	float nearest = 1e10f;
	for (unsigned i = 0; i < face.corner_count; ++i) {
		const auto &a = face.corners[i];
		const auto &b = face.corners[(i + 1) % face.corner_count];
		const auto edge = difference(b, a);
		const float squared = std::abs(dot(edge, edge));
		const float amount = squared == 0 ? 0 : dot(difference(receiver, a), edge) / squared;
		const auto point = amount < 0 ? a : amount > 1 ? b
		                                               : along(a, edge, amount);
		const auto delta = difference(point, receiver);
		const float distance = std::sqrt(dot(delta, delta));
		if (nearest > distance) {
			nearest = distance;
			result = point;
		}
	}
	return result;
}

std::optional<std::int32_t> geometry_ambient_distance(std::int32_t center_distance, float source_radius, std::int32_t segment_radius)
{
	const float radius = source_radius * 65536.0f;
	const float rounded_distance = static_cast<float>(center_distance);
	if (center_distance < 0 || !std::isfinite(radius) || radius < 0 || double(radius) > std::numeric_limits<std::int32_t>::max() ||
	    double(rounded_distance) > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("Invalid ambient light distance or radius");
	const auto distance = std::int64_t(static_cast<std::int32_t>(rounded_distance)) - segment_radius;
	if (distance < std::numeric_limits<std::int32_t>::min() || distance > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("Ambient light distance exceeds fixed-point range");
	if (distance - static_cast<std::int32_t>(radius) > 222 * 65536) return std::nullopt;
	return static_cast<std::int32_t>(distance);
}

std::vector<GeometryLightDistance> geometry_active_lights(const std::vector<GeometryLightDistance> &candidates)
{
	std::array<std::optional<GeometryLightDistance>, 4096> slots{};
	std::vector<int> used;
	for (const auto &candidate : candidates) {
		if (candidate.index < 0) throw std::runtime_error("Invalid active geometry light index");
		if (std::find(used.begin(), used.end(), candidate.index) != used.end()) continue;
		int slot = (candidate.distance / 2000 + 5) / 10;
		if (slot >= static_cast<int>(slots.size())) continue;
		slot = std::max(0, slot);
		while (slot < static_cast<int>(slots.size()) && slots[slot]) ++slot;
		if (slot == static_cast<int>(slots.size())) continue;
		slots[slot] = candidate;
		used.push_back(candidate.index);
	}
	std::vector<GeometryLightDistance> result;
	for (const auto &slot : slots)
		if (slot) result.push_back(*slot);
	return result;
}

std::vector<GeometryLightDistance> nearest_geometry_lights(std::vector<GeometryLightDistance> candidates)
{
	// Reference light indices are signed 16-bit; never narrow an unbounded vector
	if (candidates.size() > 32768) throw std::runtime_error("Geometry light candidates exceed reference index capacity");
	for (const auto &candidate : candidates)
		if (candidate.index < 0 || candidate.index > 32767) throw std::runtime_error("Invalid geometry light candidate index");
	if (!candidates.empty()) sort_geometry_distances(candidates, 0, static_cast<int>(candidates.size()) - 1);
	if (candidates.size() > 32) candidates.resize(32);
	return candidates;
}

std::optional<GeometryAttenuation> geometry_attenuation(const GeometryLightSample &input)
{
	if (input.brightness <= 0) return std::nullopt;
	float distance = input.distance * input.range_scale;
	float normal_dot = input.normal_dot;
	float attenuation = 1;
	if (distance <= 0) {
		distance = 0;
		normal_dot = 1;
	} else {
		float decay = std::min(input.emission_angle, normal_dot);
		if (decay < 0) {
			decay += 1.0000001f;
			distance /= decay * decay;
		}
		if (distance > input.cutoff) return std::nullopt;
		attenuation = 1.0f + input.linear_attenuation * distance + input.quadratic_attenuation * distance * distance;
	}
	attenuation /= input.brightness;
	return GeometryAttenuation{ distance, normal_dot, attenuation };
}

std::optional<Rgb> geometry_source_color(const GeometryColorInput &input)
{
	const auto position = input.self ? input.receiver : geometry_light_point(input.face, input.receiver);
	auto ray = difference(position, input.receiver);
	const float distance = std::sqrt(dot(ray, ray));
	auto sample = input.attenuation;
	sample.distance = distance;
	sample.normal_dot = sample.emission_angle = 1;
	if (distance != 0) {
		for (auto &axis : ray) axis /= distance;
		if (input.normal != LightPoint{}) {
			auto center_ray = difference(input.center, input.receiver);
			const float magnitude = std::sqrt(dot(center_ray, center_ray));
			if (magnitude)
				for (auto &axis : center_ray) axis /= magnitude;
			sample.normal_dot = std::max(dot(input.normal, center_ray), dot(input.normal, ray));
			if (sample.normal_dot < 0 && sample.normal_dot > -0.01f) sample.normal_dot = 0;
		}
		if (distance > 0.1f) {
			sample.emission_angle = -dot(ray, input.emission_direction);
			if (sample.emission_angle >= -0.01f) sample.emission_angle += (1 - sample.emission_angle) / distance;
		}
	}
	const auto attenuated = geometry_attenuation(sample);
	if (!attenuated) return std::nullopt;
	Rgb color = input.material_ambient;
	if (attenuated->normal_dot >= -0.01f) {
		float normal_dot = attenuated->normal_dot;
		if (attenuated->distance <= 1) normal_dot = 1;
		else normal_dot += (1 - normal_dot) / attenuated->distance;
		for (unsigned channel = 0; channel < 3; ++channel) color[channel] += input.material_diffuse[channel] * normal_dot;
	}
	for (unsigned channel = 0; channel < 3; ++channel) {
		color[channel] *= input.light_color[channel];
		color[channel] /= attenuated->attenuation;
	}
	return color;
}

void GeometryColorSum::add(const std::optional<Rgb> &color, bool self_source)
{
	if (self_source) ++self_count;
	if (!color) return;
	++contributing;
	auto &sum = self_source ? self : ordinary;
	for (unsigned channel = 0; channel < 3; ++channel) sum[channel] += (*color)[channel];
}

Rgb GeometryColorSum::finish() const
{
	auto result = ordinary;
	if (contributing) {
		if (self_count)
			for (unsigned channel = 0; channel < 3; ++channel) result[channel] += self[channel] / static_cast<float>(self_count);
		const float maximum = *std::max_element(result.begin(), result.end());
		if (maximum > 1)
			for (auto &channel : result) channel /= maximum;
	}
	return result;
}

namespace
{
void scale(Rgb &color, float factor)
{
	for (auto &channel : color) channel *= factor;
}
void cap_to_light(Rgb &color, float light)
{
	const auto maximum = *std::max_element(color.begin(), color.end());
	if (maximum > light) scale(color, light / maximum);
}
std::int32_t fixed(std::int64_t value)
{
	if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL vertex light overflow");
	return static_cast<std::int32_t>(value);
}
} // namespace

std::int32_t flare_intensity(std::int32_t light, std::int32_t life, unsigned sample)
{
	if (sample > 32767) throw std::runtime_error("Invalid XL flare random sample");
	const float intensity = (1.75f + float(sample) / (32767.0f * 4.0f)) * std::min(light, life);
	if (!std::isfinite(intensity) || double(intensity) < std::numeric_limits<std::int32_t>::min() || double(intensity) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL flare intensity overflow");
	return static_cast<std::int32_t>(intensity);
}

void DynamicVertexLight::add(std::int32_t contribution, const Rgb *source_color, bool force_color)
{
	const auto next_intensity = fixed(std::int64_t(intensity) + contribution);
	if (source_color) {
		for (const float channel : *source_color)
			if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL dynamic source color");
		const bool white = (*source_color)[0] == 1 && (*source_color)[1] == 1 && (*source_color)[2] == 1;
		if (force_color || !white) {
			if (has_color) {
				for (unsigned i = 0; i < 3; ++i) color[i] = (color[i] + (*source_color)[i]) / 2;
			} else color = *source_color;
			has_color = true;
		}
	}
	intensity = next_intensity;
}

std::vector<float> vertex_brightness(const Inventory &level)
{
	if (!((level.level_version == 15 && level.game_version == 33) || (level.level_version == 17 && level.game_version == 36)))
		throw std::runtime_error("XL brightness requires audited 15/33 or 17/36 geometry");
	// Pinned XL segment.cpp sideVertIndex; only the audited four-corner layout
	constexpr unsigned corners[6][4] = {
		{ 7, 6, 2, 3 }, { 0, 4, 7, 3 }, { 0, 1, 5, 4 }, { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 3, 2, 1, 0 }
	};
	std::vector<float> brightness(level.vertices.size(), 0);
	for (const auto &segment : level.segments) {
		for (unsigned side = 0; side < 6; ++side) {
			// Open portals have no serialized UVLs and do not contribute
			if (segment.children[side] != -1 && segment.sides[side].wall == -1) continue;
			for (unsigned corner = 0; corner < 4; ++corner) {
				const int vertex = segment.vertices[corners[side][corner]];
				const int light = segment.sides[side].uvl[corner * 3 + 2];
				if (vertex < 0 || std::size_t(vertex) >= brightness.size() || light < 0 || light > 65535)
					throw std::runtime_error("Invalid XL brightness geometry");
				// The disk light is an unsigned 16-bit half-fixed value
				brightness[vertex] = std::max(brightness[vertex], float(light * 2) / 65536);
			}
		}
	}
	return brightness;
}

ShadedVertex shade_vertex(const VertexLighting &in)
{
	if (in.blend < 0 || in.blend > 2) throw std::runtime_error("Unknown XL face blend");
	auto color = in.face;
	if (in.ambient_enabled) {
		for (unsigned i = 0; i < 3; ++i) {
			if (in.blend == 1) color[i] *= in.ambient[i];
			else if (in.blend == 2) color[i] = color[i] * in.face_alpha + in.ambient[i] * (1 - in.face_alpha);
			else color[i] += in.ambient[i];
		}
	}
	auto light = in.darkness ? 0 : in.static_light;
	if (!in.darkness && in.flashing) {
		const auto product = std::int64_t(light) * in.flash_scale;
		light = fixed(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
	}
	const float fl = float(light) / 65536, dl = float(in.dynamic_light) / 65536;
	light = fixed(std::int64_t(light) + in.dynamic_light);
	if (in.extra_game_info && in.has_dynamic_color) {
		if (in.mix_colors) {
			if (in.colored) {
				if (in.ambient_enabled && fl != 0 && in.vertex_brightness != 0) {
					scale(color, fl / in.vertex_brightness);
					for (unsigned i = 0; i < 3; ++i) color[i] += in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				} else if (in.ambient_enabled) {
					color = in.dynamic_color;
					scale(color, dl);
					cap_to_light(color, dl);
				} else {
					for (unsigned i = 0; i < 3; ++i) color[i] = fl + in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				}
			} else color.fill(fl + dl);
			if (in.cap_colors)
				for (auto &channel : color) channel = std::min(channel, 1.0f);
		} else {
			color = in.dynamic_color;
			scale(color, float(std::pow(float(light) / 65536, 1.0f / 3.0f)));
		}
	} else cap_to_light(color, fl + dl);
	scale(color, in.brightness);
	const float bright_light = float(light) * in.brightness;
	if (!std::isfinite(bright_light) || double(bright_light) < std::numeric_limits<std::int32_t>::min() || double(bright_light) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("Invalid XL vertex brightness");
	light = std::min(static_cast<std::int32_t>(bright_light), std::int32_t(65536));
	const float factor = in.scale_light ? (in.headlight ? 0.4f : 0.3f) : 1.0f;
	if (!in.face_index || !in.ambient_enabled || in.outside_endlevel)
		color.fill((in.no_bitmap_lighting ? 1.0f : float(light) / 65536) * factor);
	else if (factor != 1) scale(color, factor);
	for (const auto channel : color)
		if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL vertex color");
	return { color, light };
}
} // namespace xl_compat
