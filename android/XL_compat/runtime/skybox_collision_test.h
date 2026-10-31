// The skybox pixel consumer needs the actual triangle retained by native FVI
#include "skybox_collision.h"
extern "C" {
#include "fvi.h"
#include "gameseg.h"
#include "physics.h"
#include "console.h"
}
static void skybox_collision_face_native_case(const xl_compat::Inventory &original)
{
	unsigned checked = 0, triangles[2]{};
	int control_segment = -1;
	vms_vector control_start{}, control_end{};
	fvi_info control_hit{};
	for (int segment = 0; segment <= Highest_segment_index && checked < 1024; ++segment) {
		vms_vector center;
		compute_segment_center(&center, &Segments[segment]);
		for (int side = 0; side < 6 && checked < 1024; ++side) {
			if (Segments[segment].children[side] >= 0) continue;
			int count, vertices[6];
			create_abs_vertex_lists(&count, vertices, segment, side, __FILE__, __LINE__);
			for (int face = 0; face < count; ++face) {
				vms_vector target{};
				for (auto axis : { &vms_vector::x, &vms_vector::y, &vms_vector::z }) {
					std::int64_t sum = 0;
					for (int corner = 0; corner < 3; ++corner) sum += Vertices[vertices[face * 3 + corner]].*axis;
					target.*axis = int(sum / 3);
					target.*axis += (target.*axis - center.*axis) / 16;
				}
				fvi_query query{};
				query.p0 = &center;
				query.p1 = &target;
				query.startseg = segment;
				query.thisobjnum = -1;
				fvi_info hit{};
				const int result = find_vector_intersection(&query, &hit);
				if (result != HIT_WALL || hit.hit_side_seg != segment || hit.hit_side != side) continue;
				require(hit.hit_face == face, "Native FVI lost the selected wall triangle");
				if (control_segment < 0 && Segments[segment].objects < 0) {
					control_segment = segment;
					control_start = center;
					control_end = target;
					control_hit = hit;
				}
				++checked;
				++triangles[face];
			}
		}
	}
	require(checked >= 100 && triangles[0] && triangles[1], "Native FVI face control lacked both triangle branches");
	con_printf(CON_NORMAL, "XL skybox collision boundary: %u native wall hits retain their actual triangle (%u, %u)\n", checked, triangles[0], triangles[1]);
	require(control_segment >= 0, "Skybox physics control lacks an empty external segment");
	auto derived = original;
	for (auto &segment : derived.segments) segment.function = 10;
	xl_compat::bind_skybox_collision(derived);
	struct Guard {
		fix time = FrameTime;
		int actor = -1;
		~Guard()
		{
			if (actor >= 0) obj_delete(actor);
			FrameTime = time;
			xl_skybox_collision_clear();
		}
	} guard;
	for (int type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_WEAPON })
		for (int life : { 65535, 65536, 65537 }) {
			object owner{};
			owner.type = ubyte(type);
			owner.lifeleft = life;
			owner.flags = 0x40;
			auto hit = control_hit;
			const bool passed = xl_skybox_wall_collision(&owner, &hit, &control_end) != 0;
			const bool weapon = type == OBJ_WEAPON, retire = weapon && life > 65536;
			require(passed == weapon && hit.hit_type == (weapon ? HIT_NONE : HIT_WALL) &&
			            owner.lifeleft == (retire ? 0 : life) && owner.flags == (0x40 | (retire ? OF_SHOULD_BE_DEAD : 0)) &&
			            !std::memcmp(&hit.hit_pnt, &control_hit.hit_pnt, sizeof(hit.hit_pnt)),
			        "Native skybox wall hook changed actor policy, lifetime or source hit position");
		}
	guard.actor = obj_create(OBJ_WEAPON, 0, control_segment, &control_start, &vmd_identity_matrix, 0, CT_WEAPON, MT_PHYSICS, RT_NONE);
	require(guard.actor >= 0, "Cannot allocate skybox physics control weapon");
	auto &shot = Objects[guard.actor];
	shot.mtype.phys_info = {};
	shot.mtype.phys_info.mass = F1_0;
	shot.lifeleft = 65537;
	shot.ctype.laser_info.parent_num = ConsoleObject - Objects;
	shot.ctype.laser_info.parent_signature = ConsoleObject->signature;
	shot.ctype.laser_info.parent_type = OBJ_PLAYER;
	vm_vec_sub(&shot.mtype.phys_info.velocity, &control_end, &control_start);
	vm_vec_scale(&shot.mtype.phys_info.velocity, i2f(4));
	FrameTime = F1_0 / 4;
	do_physics_sim(&shot);
	require(shot.lifeleft == 0 && (shot.flags & OF_SHOULD_BE_DEAD), "Actual weapon physics bypassed skybox external-wall retirement");
	{
		// Exercise the real UV/pixel consumer and compiled skybox lookup together
		const int target_segment = (control_segment + 1) % Num_segments;
		for (auto &segment : derived.segments) segment.function = 0;
		derived.segments[std::size_t(target_segment)].function = 10;
		xl_compat::bind_skybox_collision(derived);
		vms_vector destination;
		compute_segment_center(&destination, &Segments[target_segment]);
		auto &surface = Segments[control_hit.hit_side_seg].sides[control_hit.hit_side];
		auto &bitmap = GameBitmaps[Textures[surface.tmap_num].index];
		struct Pixels {
			side &surface;
			grs_bitmap &bitmap;
			side saved_surface;
			grs_bitmap saved_bitmap;
			~Pixels()
			{
				surface = saved_surface;
				bitmap = saved_bitmap;
			}
		} pixels{ surface, bitmap, surface, bitmap };
		surface.tmap_num2 = 0;
		std::array<ubyte, 4> data{ 31, 71, 121, 0 };
		bitmap.bm_w = bitmap.bm_h = 1;
		bitmap.bm_flags = 0;
		bitmap.bm_data = data.data();
		for (bool rgba : { false, true })
			for (int value : { 0, 32, 254, 255 }) {
				bitmap.bm_type = rgba ? BM_RGBA : BM_LINEAR;
				bitmap.bm_rowsize = rgba ? 4 : 1;
				data[rgba ? 3 : 0] = ubyte(value);
				object owner{};
				owner.type = OBJ_WEAPON;
				owner.lifeleft = i2f(3);
				owner.flags = 0x40;
				auto hit = control_hit;
				const bool transparent = rgba ? value == 0 : value >= 254;
				const bool passed = xl_skybox_wall_collision(&owner, &hit, &destination) != 0;
				require(passed == transparent && hit.hit_seg == (transparent ? target_segment : control_segment) &&
				            hit.hit_type == (transparent ? HIT_NONE : HIT_WALL) && owner.lifeleft == i2f(3) && owner.flags == 0x40 &&
				            !std::memcmp(&hit.hit_pnt, &control_hit.hit_pnt, sizeof(hit.hit_pnt)),
				        "Native skybox entry differs for indexed/RGBA pixels, lookup, lifetime or hit position");
				hit = control_hit;
				require(!xl_skybox_wall_collision(&owner, &hit, &control_start) && hit.hit_type == HIT_WALL,
				        "Native skybox lookup accepted an ordinary destination");
			}
	}
	{
		// Drive the actual physics loop from an ordinary segment into a separate skybox
		int target = -1;
		for (int candidate = 0; candidate < Num_segments; ++candidate)
			if (candidate != control_segment && Segments[candidate].objects < 0) {
				target = candidate;
				break;
			}
		require(target >= 0 && Num_vertices + 8 <= MAX_VERTICES, "Skybox full entry requires an empty segment and eight temporary vertices");
		struct RestoreGeometry {
			int target, actor, origin;
			segment saved;
			int count = Num_vertices;
			int highest = Highest_vertex_index;
			std::array<vms_vector, 8> tail;
			RestoreGeometry(int t, int a, int o) : target(t), actor(a), origin(o), saved(Segments[t])
			{
				std::copy_n(Vertices + count, 8, tail.begin());
			}
			~RestoreGeometry()
			{
				obj_relink(actor, origin);
				Segments[target] = saved;
				std::copy(tail.begin(), tail.end(), Vertices + count);
				Num_vertices = count;
				Highest_vertex_index = highest;
			}
		} geometry{ target, guard.actor, control_segment };
		Segments[target] = Segments[control_segment];
		Segments[target].objects = -1;
		for (auto &segment : derived.segments) segment.function = 0;
		derived.segments[target] = derived.segments[control_segment];
		derived.segments[target].function = 10;
		for (int side = 0; side < 6; ++side) {
			Segments[target].children[side] = -1;
			Segments[target].sides[side].wall_num = -1;
			derived.segments[target].children[side] = -1;
			derived.segments[target].sides[side].wall = -1;
		}
		for (int corner = 0; corner < 8; ++corner) {
			vms_vector expanded;
			vm_vec_sub(&expanded, &Vertices[Segments[control_segment].verts[corner]], &control_start);
			vm_vec_scale(&expanded, i2f(4));
			vm_vec_add2(&expanded, &control_start);
			Vertices[Num_vertices] = expanded;
			Segments[target].verts[corner] = Num_vertices++;
			derived.segments[target].vertices[corner] = int(derived.vertices.size());
			derived.vertices.push_back({ expanded.x, expanded.y, expanded.z });
		}
		const auto destination = control_end;
		Highest_vertex_index = Num_vertices - 1;
		const auto entry = control_hit;
		xl_compat::bind_skybox_collision(derived);
		auto &surface = Segments[entry.hit_side_seg].sides[entry.hit_side];
		auto &bitmap = GameBitmaps[Textures[surface.tmap_num].index];
		struct RestorePixels {
			side &surface;
			grs_bitmap &bitmap;
			side saved_surface;
			grs_bitmap saved_bitmap;
			~RestorePixels()
			{
				surface = saved_surface;
				bitmap = saved_bitmap;
			}
		} pixels{ surface, bitmap, surface, bitmap };
		std::array<ubyte, 4> transparent{ 31, 71, 121, 0 };
		surface.tmap_num2 = 0;
		bitmap.bm_type = BM_RGBA;
		bitmap.bm_w = bitmap.bm_h = 1;
		bitmap.bm_rowsize = 4;
		bitmap.bm_flags = 0;
		bitmap.bm_data = transparent.data();
		shot.flags = 0;
		shot.lifeleft = i2f(3);
		shot.pos = control_start;
		obj_relink(guard.actor, control_segment);
		shot.mtype.phys_info = {};
		shot.mtype.phys_info.mass = F1_0;
		vm_vec_sub(&shot.mtype.phys_info.velocity, &destination, &control_start);
		vm_vec_scale(&shot.mtype.phys_info.velocity, i2f(4));
		FrameTime = F1_0 / 4;
		do_physics_sim(&shot);
		require(shot.segnum == target && shot.lifeleft == i2f(3) && !(shot.flags & OF_SHOULD_BE_DEAD), "Actual weapon physics did not retain skybox entry and lifetime");
		require(!std::memcmp(&shot.pos, &entry.hit_pnt, sizeof(shot.pos)) && !get_seg_masks(&shot.pos, target, 0, nullptr, 0).centermask,
		        "Skybox entry did not preserve the source wall-hit position inside its enclosing cell");
		const auto entered = shot.pos;
		do_physics_sim(&shot);
		require(shot.segnum == target && !(shot.flags & OF_SHOULD_BE_DEAD) && !get_seg_masks(&shot.pos, target, 0, nullptr, 0).centermask &&
		            std::memcmp(&shot.pos, &entered, sizeof(shot.pos)),
		        "Weapon failed to continue moving inside the skybox on the next frame");
		con_printf(CON_NORMAL, "XL skybox full entry: ordinary segment %d to skybox %d through actual FVI and physics passed\n", control_segment, target);
	}
	xl_compat::bind_skybox_collision(original);
	auto hit = control_hit;
	require(!xl_skybox_wall_collision(&shot, &hit, &control_end) && hit.hit_type == HIT_WALL, "Ordinary source retained derived skybox collision state");
	con_printf(CON_NORMAL, "XL skybox native collision: actor/lifetime boundaries, real weapon physics retirement, indexed/RGBA pixel entry, compiled lookup and ordinary-source reset passed; admission remains closed\n");
}
