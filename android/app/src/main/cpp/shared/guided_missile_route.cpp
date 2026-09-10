#include "guided_missile_route.h"

#include <algorithm>
#include <vector>
#include <cstring>

extern "C" {
#include "inferno.h"
#include "fvi.h"
#include "route_collision.h"
#include "gameseg.h"
#include "object.h"
#include "segment.h"
#include "wall.h"
#include "weapon.h"
#include "laser.h"
#include "secretarea.h"
}

namespace
{
struct route_point {
	int segment;
	vms_vector position;
};
struct route_edge {
	int from, to;
	bool direct;
	vms_vector via;
};
struct work_budget {
	int (*consume)(void *);
	void *user;
	bool exhausted = false;
};

bool spend(work_budget &budget)
{
	if (budget.exhausted) return false;
	if (budget.consume && !budget.consume(budget.user)) budget.exhausted = true;
	return !budget.exhausted;
}

bool clear_leg(work_budget &budget, int segment, vms_vector from, vms_vector to, fix radius)
{
	fvi_query query = {};
	fvi_info hit = {};
	query.p0 = &from;
	query.p1 = &to;
	query.startseg = segment;
	query.thisobjnum = -1;
	query.rad = radius;
	query.flags = FQ_TRANSPOINT;
	if (!spend(budget) || find_vector_intersection(&query, &hit) != HIT_NONE) return false;
	const vms_vector start = from;
	const int chunks = vm_vec_dist(&from, &to) / F1_0 + 1;
	for (int chunk = 1; chunk <= chunks; ++chunk) {
		vms_vector end;
		end.x = start.x + (fix) (((long long) to.x - start.x) * chunk / chunks);
		end.y = start.y + (fix) (((long long) to.y - start.y) * chunk / chunks);
		end.z = start.z + (fix) (((long long) to.z - start.z) * chunk / chunks);
		query.p0 = &from;
		query.p1 = &end;
		query.startseg = segment;
		if (!spend(budget) || find_vector_intersection(&query, &hit) != HIT_NONE) return false;
		segment = find_point_seg(&end, hit.hit_seg >= 0 ? hit.hit_seg : segment);
		if (segment < 0) return false;
		from = end;
	}
	return true;
}

bool impact_leg(work_budget &budget, int segment, vms_vector from, vms_vector to, fix radius, int wall)
{
	const vms_vector start = from;
	const int chunks = vm_vec_dist(&from, &to) / F1_0 + 1;
	for (int chunk = 1; chunk <= chunks; ++chunk) {
		vms_vector end = {
			start.x + (fix) (((long long) to.x - start.x) * chunk / chunks),
			start.y + (fix) (((long long) to.y - start.y) * chunk / chunks),
			start.z + (fix) (((long long) to.z - start.z) * chunk / chunks)
		};
		fvi_query query = {};
		fvi_info hit = {};
		query.p0 = &from;
		query.p1 = &end;
		query.startseg = segment;
		query.thisobjnum = -1;
		query.rad = radius;
		query.flags = FQ_TRANSPOINT;
		if (!spend(budget)) return false;
		const int fate = find_vector_intersection(&query, &hit);
		if (fate == HIT_WALL) return hit.hit_side_seg == Walls[wall].segnum && hit.hit_side == Walls[wall].sidenum;
		if (fate != HIT_NONE) return false;
		segment = find_point_seg(&end, hit.hit_seg >= 0 ? hit.hit_seg : segment);
		if (segment < 0) return false;
		from = end;
	}
	return false;
}

vms_vector center(int segment)
{
	vms_vector result;
	compute_segment_center(&result, &Segments[segment]);
	return result;
}

guided_missile_route_point export_point(const route_point &point)
{
	return { point.segment, { point.position.x, point.position.y, point.position.z } };
}
} // namespace

extern "C" int guided_missile_route_find(int wall_num, const int *launch_segments, int launch_count,
                                         int player_radius, int (*consume_work)(void *), void *work_user, guided_missile_route *result)
{
	if (!result) return 0;
	memset(result, 0, sizeof(*result));
	if (wall_num < 0 || wall_num >= Num_walls || !launch_segments || launch_count <= 0 || player_radius <= 0) return 0;
	const int destination = Walls[wall_num].segnum;
	const int target_side = Walls[wall_num].sidenum;
	if (destination < 0 || destination >= Num_segments || target_side < 0 || target_side >= 6) return 0;
	const fix radius = level_metadata_get_weapon_projectile_radius(GUIDEDMISS_ID);
	if (radius <= 0) return 0;
	work_budget budget = { consume_work, work_user };
	std::vector<route_edge> edges;
	std::vector<std::vector<int>> incoming(Num_segments);
	for (int segment = 0; segment < Num_segments && !budget.exhausted; ++segment) {
		for (int side = 0; side < 6 && !budget.exhausted; ++side) {
			const int child = Segments[segment].children[side];
			if (child < 0 || child >= Num_segments) continue;
			vms_vector from = center(segment), to = center(child), via;
			const bool direct = clear_leg(budget, segment, from, to, radius);
			bool clear = direct;
			compute_center_point_on_side(&via, &Segments[segment], side);
			int vertices[4];
			get_side_verts(vertices, segment, side);
			for (int u = 1; u < 8 && !clear && !budget.exhausted; ++u) {
				for (int v = 1; v < 8 && !clear && !budget.exhausted; ++v) {
					const int weights[4] = { (8 - u) * (8 - v), u * (8 - v), u * v, (8 - u) * v };
					long long x = 0, y = 0, z = 0;
					for (int k = 0; k < 4; ++k) {
						x += (long long) Vertices[vertices[k]].x * weights[k];
						y += (long long) Vertices[vertices[k]].y * weights[k];
						z += (long long) Vertices[vertices[k]].z * weights[k];
					}
					via = { (fix) (x / 64), (fix) (y / 64), (fix) (z / 64) };
					clear = clear_leg(budget, segment, from, via, radius) && clear_leg(budget, segment, via, to, radius);
				}
			}
			if (clear) {
				incoming[child].push_back((int) edges.size());
				edges.push_back({ segment, child, direct, via });
			}
		}
	}
	if (budget.exhausted) return 0;
	std::vector<int> next(Num_segments, -1), queue;
	next[destination] = -2;
	queue.push_back(destination);
	for (size_t cursor = 0; cursor < queue.size(); ++cursor) {
		for (int index : incoming[queue[cursor]]) {
			const int source = edges[index].from;
			if (next[source] != -1) continue;
			next[source] = index;
			queue.push_back(source);
		}
	}
	std::vector<unsigned char> ship_reachable(Num_segments, 0);
	for (int index = 0; index < launch_count; ++index)
		if (launch_segments[index] >= 0 && launch_segments[index] < Num_segments) ship_reachable[launch_segments[index]] = 1;
	int best_count = GUIDED_MISSILE_ROUTE_MAX_POINTS + 1;
	for (int source_index = 0; source_index < launch_count && !budget.exhausted; ++source_index) {
		const int source = launch_segments[source_index];
		if (source < 0 || source >= Num_segments || next[source] == -1) continue;
		int frontier = source;
		for (int distance = 0; distance < 3 && next[frontier] >= 0 && ship_reachable[frontier]; ++distance) frontier = edges[next[frontier]].to;
		if (ship_reachable[frontier] && frontier != destination) continue;
		std::vector<route_point> raw;
		for (int segment = source; next[segment] >= 0; segment = edges[next[segment]].to) {
			const auto &edge = edges[next[segment]];
			if (!edge.direct) raw.push_back({ edge.to, edge.via });
			raw.push_back({ edge.to, center(edge.to) });
		}
		if (raw.empty()) raw.push_back({ destination, center(destination) });
		vms_vector low = Vertices[Segments[source].verts[0]], high = low;
		for (int k = 1; k < 8; ++k) {
			const auto &vertex = Vertices[Segments[source].verts[k]];
			low.x = (std::min) (low.x, vertex.x);
			low.y = (std::min) (low.y, vertex.y);
			low.z = (std::min) (low.z, vertex.z);
			high.x = (std::max) (high.x, vertex.x);
			high.y = (std::max) (high.y, vertex.y);
			high.z = (std::max) (high.z, vertex.z);
		}
		route_point launch = { source, center(source) };
		int first = -1;
		for (int target = (int) raw.size() - 1; target >= 0 && first < 0 && !budget.exhausted; --target) {
			for (int sample = 0; sample < 344 && first < 0 && !budget.exhausted; ++sample) {
				object probe = {};
				probe.segnum = source;
				probe.size = player_radius;
				probe.pos = center(source);
				if (sample > 0) {
					int index = sample - 1;
					const int x = index % 7 + 1;
					index /= 7;
					const int y = index % 7 + 1;
					index /= 7;
					const int z = index % 7 + 1;
					probe.pos = { low.x + (high.x - low.x) * x / 8, low.y + (high.y - low.y) * y / 8, low.z + (high.z - low.z) * z / 8 };
				}
				if (find_point_seg(&probe.pos, source) != source || route_object_intersects_blocking_wall(&probe)) continue;
				if (clear_leg(budget, source, probe.pos, raw[target].position, radius)) {
					first = target;
					launch.position = probe.pos;
				}
			}
		}
		if (first < 0) continue;
		std::vector<route_point> path;
		route_point current = launch;
		while (first < (int) raw.size() && !budget.exhausted) {
			int farthest = (int) raw.size() - 1;
			while (farthest >= first && !clear_leg(budget, current.segment, current.position, raw[farthest].position, radius)) --farthest;
			if (farthest < first) {
				path.clear();
				break;
			}
			current = raw[farthest];
			current.segment = find_point_seg(&current.position, current.segment);
			if (current.segment < 0) {
				path.clear();
				break;
			}
			path.push_back(current);
			first = farthest + 1;
		}
		if (path.empty() || budget.exhausted) continue;
		vms_vector target, direction;
		compute_center_point_on_side(&target, &Segments[destination], target_side);
		vm_vec_normalized_dir(&direction, &target, &current.position);
		vm_vec_scale_add2(&target, &direction, F1_0 * 2);
		if (!impact_leg(budget, current.segment, current.position, target, radius, wall_num)) continue;
		// A direct flare is an ordinary shot, even if the graph took a detour
		if (impact_leg(budget, launch.segment, launch.position, target, level_metadata_get_weapon_projectile_radius(FLARE_ID), wall_num)) continue;
		path.push_back({ destination, target });
		long long length = 0;
		vms_vector previous = launch.position;
		for (auto point : path) {
			length += vm_vec_dist(&previous, &point.position);
			previous = point.position;
		}
		const fix max_length = fixmul(Weapon_info[GUIDEDMISS_ID].speed[Difficulty_level], Weapon_info[GUIDEDMISS_ID].lifetime);
		if (length >= max_length) continue;
		if ((int) path.size() >= best_count) continue;
		vms_vector muzzle;
		vm_vec_normalized_dir(&direction, &path.front().position, &launch.position);
		vm_vec_scale_add(&muzzle, &launch.position, &direction, player_radius + radius + F1_0);
		if (!clear_leg(budget, launch.segment, launch.position, muzzle, radius)) continue;
		best_count = (int) path.size();
		result->wall = wall_num;
		result->launch = export_point(launch);
		result->point_count = best_count;
		for (int point = 0; point < best_count; ++point) result->points[point] = export_point(path[point]);
	}
	if (budget.exhausted) {
		memset(result, 0, sizeof(*result));
		return 0;
	}
	return result->point_count > 0;
}
