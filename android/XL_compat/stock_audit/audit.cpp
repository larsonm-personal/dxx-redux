// Reuse stock headless initialization without modifying its production source
#define main stock_metadata_main
#include "../../app/src/main/cpp/headless/headless_metadata_dump_main.cpp"
#undef main
#include "../inventory.h"
#include <iterator>
#include <stdexcept>
extern "C" {
#include "robot.h"
#include "polyobj.h"
#include "weapon.h"
#include "vclip.h"
}

namespace
{
int checks = 0;
int mismatches = 0;
int dependencies = 0;
void dependency(int id, int count, const std::string &field)
{
	++dependencies;
	if (id < 0 || id >= count)
		throw std::runtime_error(field + ": unresolved ID " + std::to_string(id) + " (count " + std::to_string(count) + ")");
}
void equal(int actual, int expected, const std::string &field)
{
	++checks;
	if (actual != expected) {
		const auto message = field + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual);
		if (field.find("count") != std::string::npos || field.find("links") != std::string::npos)
			throw std::runtime_error(message);
		++mismatches;
		fprintf(stderr, "Mismatch: %s\n", message.c_str());
	}
}
void sequence(const std::vector<int> &actual, const std::vector<int> &expected, const std::string &label)
{
	equal(static_cast<int>(actual.size()), static_cast<int>(expected.size()), label + " count");
	for (std::size_t i = 0; i < actual.size(); ++i) equal(actual[i], expected[i], label + " field " + std::to_string(i));
}
} // namespace

int main(int argc, char **argv)
{
	try {
		const char *source = find_arg_value(argc, argv, "-xl-source");
		const char *level = find_arg_value(argc, argv, "-probe-level");
		if (!source || !level) throw std::runtime_error("Requires -xl-source original.rl2 -probe-level xlprobe.rl2 and stock -hogdir/-extra-dir/-mission arguments");
		std::ifstream input(source, std::ios::binary);
		if (!input) throw std::runtime_error("Cannot read original level");
		xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		const auto expected = xl_compat::inspect_level(bytes);
		char error[256] = {};
		if (!init_headless_metadata_runtime(argc, argv, error, sizeof(error)) ||
		    !mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)) ||
		    !load_base_mission(find_arg_value(argc, argv, "-mission"), error, sizeof(error)))
			throw std::runtime_error(error);
		if (load_level(level)) throw std::runtime_error("Stock level load failed");
		// Check original IDs, not loader-normalized IDs: stock can substitute bad IDs
		for (std::size_t i = 0; i < expected.objects.size(); ++i) {
			const auto &o = expected.objects[i];
			const auto label = "object " + std::to_string(i);
			if (o.type == OBJ_ROBOT) {
				dependency(o.id, N_robot_types, label + " robot");
				if (!robot_info_is_usable(o.id)) throw std::runtime_error(label + " unusable robot definition");
				dependency(Robot_info[o.id].model_num, N_polygon_models, label + " robot model");
				dependency(Robot_info[o.id].weapon_type, N_weapon_types, label + " robot weapon");
				if (Robot_info[o.id].weapon_type2 != -1)
					dependency(Robot_info[o.id].weapon_type2, N_weapon_types, label + " robot secondary weapon");
			} else if (o.type == OBJ_POWERUP) {
				dependency(o.id, N_powerup_types, label + " powerup");
				dependency(Powerup_info[o.id].vclip_num, Num_vclips, label + " powerup animation");
			} else if (o.type == OBJ_WEAPON) {
				dependency(o.id, N_weapon_types, label + " weapon");
			}
			if (o.contains_count > 0) {
				if (o.contains_type == OBJ_ROBOT) dependency(o.contains_id, N_robot_types, label + " contained robot");
				else if (o.contains_type == OBJ_POWERUP) dependency(o.contains_id, N_powerup_types, label + " contained powerup");
				else throw std::runtime_error(label + " unknown contained object type");
			}
		}
		equal(Highest_object_index + 1, static_cast<int>(expected.objects.size()), "object count");
		for (std::size_t i = 0; i < expected.objects.size(); ++i) {
			const auto &a = Objects[i];
			const auto &b = expected.objects[i];
			const auto label = "object " + std::to_string(i);
			equal(a.type, b.type, label + " type");
			equal(a.id, b.id, label + " ID");
			equal(a.control_type, b.control, label + " control");
			equal(a.movement_type, b.movement, label + " movement");
			equal(a.render_type, b.render, label + " render");
			equal(a.flags, b.flags, label + " flags");
			equal(a.segnum, b.segment, label + " segment");
			equal(a.pos.x, b.position[0], label + " x");
			equal(a.pos.y, b.position[1], label + " y");
			equal(a.pos.z, b.position[2], label + " z");
			equal(a.contains_type, b.contains_type, label + " contains type");
			equal(a.contains_id, b.contains_id, label + " contains ID");
			equal(a.contains_count, b.contains_count, label + " contains quantity");
			equal(a.size, b.size, label + " size");
			equal(a.shields, b.shields, label + " shields");
			const auto &m = a.orient;
			sequence({ m.rvec.x, m.rvec.y, m.rvec.z, m.uvec.x, m.uvec.y, m.uvec.z, m.fvec.x, m.fvec.y, m.fvec.z },
			         { b.orientation.begin(), b.orientation.end() }, label + " orientation");
			sequence({ a.last_pos.x, a.last_pos.y, a.last_pos.z }, { b.last_position.begin(), b.last_position.end() }, label + " last position");
			if (b.movement == MT_PHYSICS) {
				const auto &p = a.mtype.phys_info;
				sequence({ p.velocity.x, p.velocity.y, p.velocity.z, p.thrust.x, p.thrust.y, p.thrust.z,
				           p.mass, p.drag, p.brakes, p.rotvel.x, p.rotvel.y, p.rotvel.z,
				           p.rotthrust.x, p.rotthrust.y, p.rotthrust.z, p.turnroll, p.flags },
				         b.movement_data, label + " physics");
			} else if (b.movement == MT_SPINNING) {
				const auto &p = a.mtype.spin_rate;
				sequence({ p.x, p.y, p.z }, b.movement_data, label + " spin");
			}
			if (b.control == CT_AI) {
				const auto &ai = a.ctype.ai_info;
				std::vector<int> values{ ai.behavior };
				for (const auto flag : ai.flags) values.push_back(static_cast<unsigned char>(flag));
				values.insert(values.end(), { ai.hide_segment, ai.hide_index, ai.path_length, ai.cur_path_index });
				sequence(values, b.control_data, label + " AI");
			} else if (b.control == CT_POWERUP) {
				sequence({ a.ctype.powerup_info.count }, b.control_data, label + " pickup");
			}
			if (b.render == RT_POLYOBJ || b.render == RT_MORPH) {
				const auto &p = a.rtype.pobj_info;
				std::vector<int> values{ p.model_num };
				for (const auto &angle : p.anim_angles) values.insert(values.end(), { angle.p, angle.b, angle.h });
				values.insert(values.end(), { p.subobj_flags, p.tmap_override });
				sequence(values, b.render_data, label + " polygon");
			} else if (b.render == RT_WEAPON_VCLIP || b.render == RT_HOSTAGE || b.render == RT_POWERUP || b.render == RT_FIREBALL) {
				const auto &v = a.rtype.vclip_info;
				sequence({ v.vclip_num, v.frametime, static_cast<unsigned char>(v.framenum) }, b.render_data, label + " animation");
			}
		}
		equal(Highest_vertex_index + 1, static_cast<int>(expected.vertices.size()), "vertex count");
		equal(Highest_segment_index + 1, static_cast<int>(expected.segments.size()), "segment count");
		for (std::size_t i = 0; i < expected.vertices.size(); ++i) {
			const int xyz[] = { Vertices[i].x, Vertices[i].y, Vertices[i].z };
			for (int k = 0; k < 3; ++k) equal(xyz[k], expected.vertices[i][k], "vertex " + std::to_string(i));
		}
		for (std::size_t i = 0; i < expected.segments.size(); ++i) {
			const auto &s = Segments[i];
			const auto &e = expected.segments[i];
			const auto label = "segment " + std::to_string(i);
			equal(s.special, e.function, label + " function");
			equal(s.matcen_num, e.producer, label + " producer");
			equal(s.value, e.value, label + " value");
			equal(s.s2_flags, e.flags, label + " flags");
			equal(s.static_light, e.light, label + " light");
			for (int k = 0; k < 8; ++k) equal(s.verts[k], e.vertices[k], label + " vertex");
			for (int k = 0; k < 6; ++k) {
				equal(s.children[k], e.children[k], label + " child");
				const auto &a = s.sides[k];
				const auto &b = e.sides[k];
				const auto side_label = label + " side " + std::to_string(k);
				equal(a.wall_num, b.wall, side_label + " wall");
				if (e.children[k] != -1 && b.wall == -1) continue;
				equal(a.tmap_num, b.texture, side_label + " texture");
				equal(static_cast<unsigned short>(a.tmap_num2), b.overlay, side_label + " overlay");
				for (int uv = 0; uv < 4; ++uv) {
					equal(a.uvls[uv].u, b.uvl[uv * 3] * 32, side_label + " u");
					equal(a.uvls[uv].v, b.uvl[uv * 3 + 1] * 32, side_label + " v");
					equal(a.uvls[uv].l, b.uvl[uv * 3 + 2] * 2, side_label + " light");
				}
			}
		}
		equal(Num_walls, static_cast<int>(expected.walls.size()), "wall count");
		for (int i = 0; i < Num_walls; ++i) {
			const auto &a = Walls[i];
			const auto &b = expected.walls[i];
			const int actual[] = { a.segnum, a.sidenum, a.hps, a.linked_wall, a.type, a.flags, a.state, static_cast<unsigned char>(a.trigger), a.clip_num, a.keys, static_cast<unsigned char>(a.cloak_value) };
			const int wanted[] = { b.segment, b.side, b.hitpoints, b.linked_wall, b.type, b.flags, b.state, b.trigger, b.clip, b.keys, b.cloak };
			for (int k = 0; k < 11; ++k) equal(actual[k], wanted[k], "wall " + std::to_string(i) + " field " + std::to_string(k));
		}
		equal(Num_triggers, static_cast<int>(expected.triggers.size()), "trigger count");
		for (int i = 0; i < Num_triggers; ++i) {
			const auto &a = Triggers[i];
			const auto &b = expected.triggers[i];
			const auto label = "trigger " + std::to_string(i);
			equal(a.type, b.type, label + " type");
			equal(a.flags, b.flags, label + " flags");
			equal(a.value, b.value, label + " value");
			equal(a.time, b.time, label + " time");
			equal(a.num_links, static_cast<int>(b.targets.size()), label + " links");
			for (int k = 0; k < a.num_links; ++k) {
				equal(a.seg[k], b.targets[k][0], label + " target segment");
				equal(a.side[k], b.targets[k][1], label + " target side");
			}
		}
		printf("Stock field audit: %d comparisons, %d mismatches, %d dependency checks; gameplay, full object state and XL lighting remain unverified\n", checks, mismatches, dependencies);
		return mismatches ? 1 : 0;
	} catch (const std::exception &e) {
		fprintf(stderr, "Stock field audit FAILED: %s\n", e.what());
		return 1;
	}
}
