// Exercise the real loader wrapper; no renderer or gameplay certification
#include "native_crash_diagnostic.h"
#include "changing_light.h"
#include "flicker.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <cstring>
// Parse JSON types before engine headers select byte packing on MSVC
#include <nlohmann/json.hpp>
#include "named_audio.h"
#include "named_audio_state.h"
#include "audio_resource.h"
#include "classic_audio.h"
#include "../wave.h"
#include "picosha2.h"
#ifdef USE_SDLMIXER
#include <SDL_mixer.h>
#endif
extern "C" {
#include "physfsx.h"
}
#undef min
#undef max
static void xl_test_filesystem_init(int argc, char **argv)
{
	PHYSFSX_init(argc, argv);
	const char *user_dir = nullptr, *extra_dir = nullptr;
	for (int i = 1; i + 1 < argc; ++i) {
		if (!std::strcmp(argv[i], "-userdir")) user_dir = argv[i + 1];
		if (!std::strcmp(argv[i], "-extra-dir")) extra_dir = argv[i + 1];
	}
	if (!user_dir || !extra_dir || std::filesystem::weakly_canonical(user_dir) != std::filesystem::weakly_canonical(std::filesystem::path(extra_dir) / "preview-user"))
		throw std::runtime_error("test write directory must be private to fixture");
	std::filesystem::create_directories(user_dir);
	if (!PHYSFS_setWriteDir(user_dir) || !PHYSFS_addToSearchPath(user_dir, 0))
		throw std::runtime_error("cannot select isolated test write directory before log initialization");
}
#define PHYSFSX_init xl_test_filesystem_init
#define main         stock_metadata_main
#include "../../app/src/main/cpp/headless/headless_metadata_dump_main.cpp"
#undef main
#undef PHYSFSX_init
#include "preview.h"
#include "asset_dependencies.h"
#include "checkpoint_export.h"
#include "../probe.h"
#include "../lighting.h"
#include "../tests/runtime_layout_fixture.h"
#include "../producer_layout.h"
#include <iterator>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <set>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <cmath>

extern "C" {
#include "render.h"
#include "lighting.h"
#include "weapon.h"
#include "laser.h"
#include "vclip.h"
#include "timer.h"
#include "controls.h"
#include "kconfig.h"
#include "config.h"
#include "ogl_init.h"
#include "rng_shared.h"
#include "state.h"
#include "playsave.h"
#include "input_demo_start.h"
#include "input_demo_newdemo_shared.h"
#include "input_demo_hooks.h"
#include "fvi.h"
#include "gameseg.h"
#include "textures.h"
#include "ai.h"
extern fix flash_scale, Seismic_tremor_magnitude;
extern int Do_dynamic_light;
void GameProcessFrame(void);
void do_weapon_n_item_stuff(void);
void set_ambient_sound_flags(void);
void add_side_as_2_triangles(segment *sp, int sidenum);
}

extern "C" int xl_preview_reference_fans(void)
{
	return 0;
}

extern "C" int xl_preview_split_face_visible(int, int, int32_t, int32_t, int32_t)
{
	std::fputs("Unexpected face visibility query in headless XL lifecycle test\n", stderr);
	std::abort();
}

extern "C" void xl_preview_before_swap(void)
{
	std::fputs("Unexpected rendering in headless XL lifecycle test\n", stderr);
	std::abort();
}

namespace
{
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
#include "changing_light_test_records.h"
// Traverse loaded object-data references, including dependencies created after startup
// This does not prove audible playback or override precedence
using xl_assets::ObjectDependencies;

void object_dependencies()
{
	ObjectDependencies graph;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &o = Objects[i];
		if (o.type == OBJ_NONE) continue;
		if (o.type == OBJ_ROBOT) graph.robot(o.id);
		if (o.type == OBJ_POWERUP) graph.powerup(o.id);
		if (o.type == OBJ_WEAPON) graph.weapon(o.id);
		if (o.type == OBJ_CNTRLCEN) graph.weapon(CONTROLCEN_WEAPON_NUM);
		if (o.contains_count > 0) graph.contents(o.contains_type, o.contains_id);
		if (o.render_type == RT_POLYOBJ) graph.model(o.rtype.pobj_info.model_num);
		if (o.render_type == RT_POWERUP || o.render_type == RT_HOSTAGE || o.render_type == RT_FIREBALL)
			graph.clip(o.rtype.vclip_info.vclip_num);
	}
	// Conservative player arsenal coverage includes pickups and replacement loot
	for (const auto id : Primary_weapon_to_weapon_info) graph.weapon(id);
	for (const auto id : Secondary_weapon_to_weapon_info) graph.weapon(id);
	for (int id = LASER_ID_L1; id <= LASER_ID_L4; ++id) graph.weapon(id);
	graph.weapon(LASER_ID_L5);
	graph.weapon(LASER_ID_L6);
	graph.weapon(FLARE_ID);
	graph.powerup(POW_ENERGY);
	graph.powerup(POW_SHIELD_BOOST);
	graph.powerup(POW_VULCAN_AMMO);
	for (int seg = 0; seg <= Highest_segment_index; ++seg)
		for (const auto &side : Segments[seg].sides) {
			graph.texture(side.tmap_num);
			if (side.tmap_num2) graph.texture(side.tmap_num2 & 0x3fff);
		}
	for (int wall = 0; wall < Num_walls; ++wall) graph.wall_clip(Walls[wall].clip_num);
	std::cout << "Loaded object dependency graph: " << graph.robots.size() << " robots, " << graph.weapons.size() << " weapons, "
	          << graph.powerups.size() << " powerups, " << graph.models.size() << " models, " << graph.clips.size() << " vclips, "
	          << graph.bitmaps.size() << " bitmaps paged, " << graph.sounds.size() << " sound samples resolved; audible playback/override closure unverified\n";
	std::cout << "Environment dependencies: " << graph.textures.size() << " textures, " << graph.effects.size()
	          << " effects, " << graph.wall_clips.size() << " wall animations\n";
	// Prove a missing transitive weapon fails even though the placed robot exists
	require(!graph.robots.empty(), "pilot dependency fixture contains no robots");
	const int robot = *graph.robots.begin();
	const auto original_weapon = Robot_info[robot].weapon_type;
	Robot_info[robot].weapon_type = 127;
	bool rejected = false;
	try {
		ObjectDependencies invalid;
		invalid.robot(robot);
	} catch (const std::runtime_error &e) {
		rejected = !std::strcmp(e.what(), "object dependency references missing weapon");
	}
	Robot_info[robot].weapon_type = original_weapon;
	require(rejected, "missing transitive robot weapon was not rejected");
}
void depth_policy(bool pilot)
{
	const int saved = GameCfg.ClassicDepth;
	for (const int requested : { 0, 1 }) {
		GameCfg.ClassicDepth = requested;
		require(ogl_use_classic_depth() == (pilot ? 0 : requested), "incorrect pilot/stock depth pipeline");
		require(GameCfg.ClassicDepth == requested, "depth policy changed stored preference");
	}
	GameCfg.ClassicDepth = saved;
}
void unchanged(int vertex)
{
	require(!xl_preview_pilot_active() && !xl_preview_pilot_completed(), "pilot admission/completion leaked into stock level");
	depth_policy(false);
	xl_texture_emitter emitter{};
	require(!xl_preview_texture_emitter(3, 2, &emitter), "XL texture emitter leaked into inactive state");
	require(xl_preview_corona_draws() == 0, "XL corona diagnostics leaked into inactive state");
	require(xl_preview_corona_depth_failures() == 0, "XL depth diagnostics leaked into inactive state");
	xl_preview_begin_face(99, 4);
	require(xl_preview_alpha(0.73f) == 0.73f, "XL material state leaked into stock level");
	xl_preview_end_face();
	int32_t r = 101, g = 202, b = 303;
	xl_preview_vertex(vertex, 65536, &r, &g, &b);
	require(r == 101 && g == 202 && b == 303, "XL colors leaked into inactive state");
}
void materials(const xl_compat::Inventory &source)
{
	int translucent = 0, sentinel = 0;
	for (const auto &w : source.walls) {
		if (!w.cloak) continue;
		xl_preview_begin_face(w.segment, w.side);
		if (w.cloak == 21) {
			require(xl_preview_alpha(1) == 10.0f / 31.0f, "authored opacity 21 did not use XL fade range");
			const int doorway = WALL_IS_DOORWAY(&Segments[w.segment], w.side);
			require((doorway & (WID_FLY_FLAG | WID_RENDPAST_FLAG)) == (WID_FLY_FLAG | WID_RENDPAST_FLAG), "translucent illusion lost traversal or visibility");
			++translucent;
		} else if (w.cloak == 31) {
			require(xl_preview_alpha(0.25f) == 1, "XL opacity sentinel treated as transparent");
			++sentinel;
		}
		xl_preview_end_face();
		require(xl_preview_alpha(0.73f) == 0.73f, "material state leaked past face");
	}
	require(translucent == 12 && sentinel == 2, "pilot material fixture changed");
}
void material_passage(const xl_compat::Inventory &source)
{
	fix player_radius = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_PLAYER) {
			player_radius = Objects[i].size;
			break;
		}
	require(player_radius > 0, "pilot has no player collision radius");
	int checked = 0;
	for (const auto &w : source.walls) {
		if (!w.cloak) continue;
		const int child = Segments[w.segment].children[w.side];
		require(child >= 0 && child <= Highest_segment_index, "authored illusion has no destination segment");
		const int wall = Segments[w.segment].sides[w.side].wall_num;
		require(wall >= 0 && wall < Num_walls && Walls[wall].type == WALL_ILLUSION, "authored material is not the expected illusion");
		vms_vector start{}, end{};
		compute_segment_center(&start, &Segments[w.segment]);
		compute_segment_center(&end, &Segments[child]);
		for (const fix radius : { 0, F1_0 / 8, player_radius }) {
			fvi_query query{};
			query.p0 = &start;
			query.p1 = &end;
			query.startseg = w.segment;
			query.rad = radius;
			query.thisobjnum = -1;
			query.flags = FQ_GET_SEGLIST;
			fvi_info hit{};
			require(find_vector_intersection(&query, &hit) == HIT_NONE && hit.hit_seg == child,
			        "authored illusion blocked collision trace");
			// The same trace must hit this boundary when it is a closed wall
			const auto original_type = Walls[wall].type;
			Walls[wall].type = WALL_CLOSED;
			const int blocked = find_vector_intersection(&query, &hit);
			Walls[wall].type = original_type;
			require(blocked == HIT_WALL && hit.hit_side_seg == w.segment && hit.hit_side == w.side,
			        "collision control did not hit the tested material boundary");
		}
		++checked;
	}
	require(checked == 14, "pilot collision material fixture changed");
	std::cout << "All 14 authored-opacity illusions pass point, small and player-radius traces with closed-wall negative controls; object collisions and combat unverified\n";
}
// Controlled probes, separate from the untouched normal-input progression runs
void texture_emitters(const xl_compat::Inventory &source)
{
	int colored_count = 0, total = 0;
	int visible = 0, blocked_controls = 0;
	for (int segment = 0; segment < Num_segments; ++segment) {
		for (int side = 0; side < 6; ++side) {
			xl_texture_emitter emitter{};
			if (!xl_preview_texture_emitter(segment, side, &emitter)) continue;
			++total;
			const auto &rgb = source.texture_colors.at(emitter.texture).rgb;
			for (unsigned c = 0; c < 3; ++c)
				require(emitter.rgb[c] == float(rgb[c]), "live emitter lost authored texture RGB");
			if (rgb[0] != rgb[1] || rgb[1] != rgb[2]) ++colored_count;
			const auto original = emitter;
			vms_vector eye{};
			compute_segment_center(&eye, &Segments[segment]);
			const int32_t eye_position[3] = { eye.x, eye.y, eye.z };
			int32_t center[3], radius;
			float facing;
			if (xl_preview_corona_geometry(segment, side, eye_position, segment, center, &radius, &facing)) {
				++visible;
				require(radius > 0 && facing > 0, "visible corona has invalid geometry");
			}
			for (int portal = 0; portal < 6; ++portal) {
				const int neighbor = Segments[segment].children[portal];
				if (neighbor < 0 || neighbor >= Num_segments) continue;
				int opposite = -1;
				for (int s = 0; s < 6; ++s)
					if (Segments[neighbor].children[s] == segment) opposite = s;
				if (opposite < 0 || Segments[neighbor].sides[opposite].wall_num >= 0) continue;
				compute_segment_center(&eye, &Segments[neighbor]);
				const int32_t neighbor_eye[3] = { eye.x, eye.y, eye.z };
				if (!xl_preview_corona_geometry(segment, side, neighbor_eye, neighbor, center, &radius, &facing)) continue;
				require(Num_walls < MAX_WALLS, "no temporary wall slot for corona occlusion control");
				const int slot = Num_walls++;
				Walls[slot] = {};
				Walls[slot].segnum = neighbor;
				Walls[slot].sidenum = opposite;
				Walls[slot].type = WALL_CLOSED;
				Segments[neighbor].sides[opposite].wall_num = slot;
				const int seen = xl_preview_corona_geometry(segment, side, neighbor_eye, neighbor, center, &radius, &facing);
				Segments[neighbor].sides[opposite].wall_num = -1;
				--Num_walls;
				require(!seen, "solid intervening wall did not occlude corona");
				++blocked_controls;
			}
			subtract_light(segment, side);
			require(!xl_preview_texture_emitter(segment, side, &emitter), "subtracted light retained its emitter");
			add_light(segment, side);
			require(xl_preview_texture_emitter(segment, side, &emitter), "restored light lost its emitter");
			require(emitter.texture == original.texture && emitter.intensity == original.intensity, "light restoration changed emitter selection");
			auto &surface = Segments[segment].sides[side];
			const auto base = surface.tmap_num, overlay = surface.tmap_num2;
			surface.tmap_num = surface.tmap_num2 = 0;
			require(!xl_preview_texture_emitter(segment, side, &emitter), "removed luminous textures retained their emitter");
			surface.tmap_num = base;
			surface.tmap_num2 = overlay;
			if (surface.wall_num >= 0) {
				auto &w = Walls[surface.wall_num];
				const auto flags = w.flags;
				w.flags |= WALL_ILLUSION_OFF;
				require(!xl_preview_texture_emitter(segment, side, &emitter), "disabled illusion retained its emitter");
				w.flags = flags;
				const auto type = w.type;
				w.type = WALL_CLOAKED;
				require(!xl_preview_texture_emitter(segment, side, &emitter), "cloaked wall retained its emitter");
				w.type = type;
			}
		}
	}
	std::cout << "Texture emitters: total=" << total << " chromatic=" << colored_count << "; native light switching and texture/wall removal verified; rendering unverified\n";
	// Independent native inventory plus pinned default brightness audit predicts 24
	require(colored_count == 24, "authored chromatic emitter candidates differ from source audit");
	std::cout << "Corona geometry: visible=" << visible << " solid-wall controls=" << blocked_controls << "; GPU output unverified\n";
	require(visible > 0 && blocked_controls > 0, "corona geometry controls did not exercise visible and occluded paths");
}

void material_targeting(const xl_compat::Inventory &source)
{
	int robot_id = -1, parent = -1;
	std::vector<int> ignored;
	for (int i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type == OBJ_ROBOT) robot_id = Objects[i].id;
		if (Objects[i].type == OBJ_PLAYER) parent = i;
		ignored.push_back(i);
	}
	ignored.push_back(-1);
	require(robot_id >= 0 && parent >= 0, "material targeting requires pilot robot and player definitions");
	const auto previous_belief = Believed_player_pos;
	int checked = 0;
	for (const auto &w : source.walls) {
		if (!w.cloak) continue;
		auto &segment = Segments[w.segment];
		const auto child = segment.children[w.side];
		vms_vector start{}, end{}, direction{};
		compute_segment_center(&start, &segment);
		compute_segment_center(&end, &Segments[child]);
		vm_vec_normalized_dir(&direction, &end, &start);
		vms_matrix facing{};
		vm_vector_2_matrix(&facing, &direction, nullptr, nullptr);
		const int observer = obj_create(OBJ_ROBOT, robot_id, w.segment, &start, &facing, F1_0, CT_AI, MT_NONE, RT_NONE);
		const int target = obj_create(OBJ_ROBOT, robot_id, child, &end, &facing, F1_0, CT_NONE, MT_NONE, RT_NONE);
		const int projectile = obj_create(OBJ_WEAPON, 0, w.segment, &start, &facing, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
		require(observer >= 0 && target >= 0 && projectile >= 0, "cannot allocate controlled material probes");
		Objects[projectile].ctype.laser_info.parent_type = OBJ_PLAYER;
		Objects[projectile].ctype.laser_info.parent_num = parent;
		Objects[projectile].ctype.laser_info.parent_signature = Objects[parent].signature;
		// Only the controlled target participates in the projectile query
		ignored.insert(ignored.end() - 1, observer);
		Believed_player_pos = end;
		auto eye = start;
		const int visible = player_is_visible_from_object(&Objects[observer], &eye, 0, &direction);
		const int object_visible = object_to_object_visibility(&Objects[observer], &Objects[target], FQ_TRANSWALL);
		fvi_query query{};
		query.p0 = &start;
		query.p1 = &end;
		query.startseg = w.segment;
		query.rad = Objects[projectile].size;
		query.thisobjnum = projectile;
		query.ignore_obj_list = ignored.data();
		query.flags = FQ_CHECK_OBJS | FQ_TRANSPOINT;
		fvi_info hit{};
		const int fate = find_vector_intersection(&query, &hit);
		const int hit_object = hit.hit_object;
		// Remove the portal for the control, guaranteeing a solid boundary even
		// when the original texture has transparent pixels or authored opacity
		segment.children[w.side] = -1;
		const int blocked_visibility = player_is_visible_from_object(&Objects[observer], &eye, 0, &direction);
		const int blocked_object_visibility = object_to_object_visibility(&Objects[observer], &Objects[target], FQ_TRANSWALL);
		const int blocked_fate = find_vector_intersection(&query, &hit);
		segment.children[w.side] = child;
		ignored.erase(ignored.end() - 2);
		obj_delete(projectile);
		obj_delete(target);
		obj_delete(observer);
		require(visible == 2 && object_visible == 1, "authored illusion blocked native AI/object visibility");
		require(fate == HIT_OBJECT && hit_object == target, "weapon query did not hit the controlled target through authored illusion");
		require(blocked_visibility == 0 && blocked_object_visibility == 0 && blocked_fate == HIT_WALL && hit.hit_side_seg == w.segment && hit.hit_side == w.side,
		        "material targeting negative control did not stop at the solid boundary");
		++checked;
	}
	Believed_player_pos = previous_belief;
	require(checked == 14, "pilot targeting material fixture changed");
	std::cout << "All 14 authored-opacity illusions pass native AI/object visibility and weapon-object collision queries with solid-boundary controls; controlled fixtures, not live combat\n";
}
void colored(const xl_compat::Inventory &source, int vertex)
{
	xl_compat::VertexLighting input;
	for (unsigned i = 0; i < 3; ++i) input.ambient[i] = float(source.vertex_colors[vertex].rgb[i]);
	input.static_light = 65536;
	const auto expected = xl_compat::shade_vertex(input);
	int32_t r = -1, g = -1, b = -1;
	xl_preview_vertex(vertex, 65536, &r, &g, &b);
	require(r == int32_t(expected.rgb[0] * 65536) && g == int32_t(expected.rgb[1] * 65536) && b == int32_t(expected.rgb[2] * 65536), "XL preview colors not applied");
}
void flash_lighting(int vertex)
{
	const auto previous_flash = flash_scale, previous_seismic = Seismic_tremor_magnitude;
	const int previous_reactor = Control_center_destroyed;
	Control_center_destroyed = 0;
	Seismic_tremor_magnitude = 0;
	flash_scale = 65536;
	std::array<int32_t, 3> normal{}, dim{}, seismic{};
	xl_preview_vertex(vertex, 32768, &normal[0], &normal[1], &normal[2]);
	Control_center_destroyed = 1;
	flash_scale = 32768;
	xl_preview_vertex(vertex, 32768, &dim[0], &dim[1], &dim[2]);
	std::cout << "XL flash probe normal=" << normal[0] << ',' << normal[1] << ',' << normal[2] << " dim=" << dim[0] << ',' << dim[1] << ',' << dim[2] << '\n';
	const auto peak = *std::max_element(dim.begin(), dim.end());
	// Float color normalization followed by fixed truncation can lose one unit
	require(peak >= 16383 && peak <= 16384, "reactor flash did not reach colored light");
	require(dim != normal, "reactor flash was discarded");
	Control_center_destroyed = 0;
	Seismic_tremor_magnitude = 65536;
	xl_preview_vertex(vertex, 32768, &seismic[0], &seismic[1], &seismic[2]);
	require(seismic == dim, "seismic flash used a different scalar path");
	Control_center_destroyed = previous_reactor;
	Seismic_tremor_magnitude = previous_seismic;
	flash_scale = previous_flash;
}
void dynamic_lighting(const xl_compat::Inventory &source, int vertex)
{
	const auto first = Objects[0];
	std::vector<int> types;
	for (int i = 0; i <= Highest_object_index; ++i) {
		types.push_back(Objects[i].type);
		Objects[i].type = OBJ_NONE;
	}
	const int previous_count = N_render_segs, previous_enabled = Do_dynamic_light;
	std::vector<int> render_list(Render_list, Render_list + Num_segments);
	N_render_segs = Num_segments;
	for (int i = 0; i < Num_segments; ++i) Render_list[i] = i;
	Do_dynamic_light = 1;
	Objects[0].type = OBJ_LIGHT;
	Objects[0].segnum = 0;
	Objects[0].pos = Vertices[vertex];
	Objects[0].ctype.light_info.intensity = F1_0;
	xl_preview_dynamic_frame();
	require(xl_preview_unresolved_emitters() == 0, "controlled point emitter marked unresolved");
	std::array<int32_t, 3> rgb{};
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	const auto peak = *std::max_element(rgb.begin(), rgb.end());
	require(peak >= 16383 && peak <= 16384, "live point light did not reach colored shading");
	Objects[0].type = OBJ_WEAPON;
	Objects[0].id = OMEGA_ID;
	const int mode_before_omega = Game_mode;
	Game_mode |= GM_MULTI;
	xl_preview_dynamic_frame();
	Game_mode = mode_before_omega;
	require(xl_preview_unresolved_emitters() == 1, "unsupported weapon emitter silently omitted");
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "dynamic light leaked across frames");
	const auto brightness = xl_compat::vertex_brightness(source);
	auto check_color = [&](int v, int32_t intensity, xl_compat::Rgb color) {
		xl_compat::VertexLighting input;
		for (unsigned c = 0; c < 3; ++c) input.ambient[c] = float(source.vertex_colors[v].rgb[c]);
		input.dynamic_light = intensity;
		input.dynamic_color = color;
		input.has_dynamic_color = input.extra_game_info = true;
		input.vertex_brightness = brightness[v];
		const auto expected = xl_compat::shade_vertex(input);
		xl_preview_vertex(v, 0, &rgb[0], &rgb[1], &rgb[2]);
		for (unsigned c = 0; c < 3; ++c)
			require(rgb[c] == int32_t(expected.rgb[c] * 65536), "live emitter intensity/color differs from reference rule");
	};
	unsigned fx_state = 0, sim_state = 0, sim_after = 0;
	require(d_rand_get_stream_state(D_RNG_FX, &fx_state) && d_rand_get_stream_state(D_RNG_SIM, &sim_state), "RNG state unavailable for flare isolation check");
	const auto fx_calls = d_rand_get_stream_call_count(D_RNG_FX), sim_calls = d_rand_get_stream_call_count(D_RNG_SIM);
	const auto flare_light = Weapon_info[FLARE_ID].light;
	Weapon_info[FLARE_ID].light = F1_0;
	Objects[0].id = FLARE_ID;
	for (int life : { 2 * F1_0, F1_0 / 2 }) {
		Objects[0].lifeleft = life;
		d_srand_stream(D_RNG_FX, 12345);
		const unsigned sample = d_rand_stream(D_RNG_FX);
		d_srand_stream(D_RNG_FX, 12345);
		const auto calls = d_rand_get_stream_call_count(D_RNG_FX);
		xl_preview_dynamic_frame();
		require(xl_preview_unresolved_emitters() == 0, "flare emitter unresolved");
		require(d_rand_get_stream_call_count(D_RNG_FX) == calls + 1, "flare did not consume exactly one FX sample");
		check_color(vertex, xl_compat::flare_intensity(F1_0, life, sample) / 4, { 1, 1, 1 });
	}
	require(d_rand_get_stream_state(D_RNG_SIM, &sim_after) && sim_after == sim_state && d_rand_get_stream_call_count(D_RNG_SIM) == sim_calls, "flare rendering advanced simulation RNG");
	d_rand_set_stream_state(D_RNG_FX, fx_state);
	d_rand_set_stream_call_count(D_RNG_FX, fx_calls);
	Weapon_info[FLARE_ID].light = flare_light;
	// Controlled weapon-table intensity isolates color selection from retail values
	for (int id : { int(CONCUSSION_ID), int(SUPER_MECH_MISS), int(LASER_ID), int(SUPERPROX_ID) }) {
		const auto previous_light = Weapon_info[id].light;
		Weapon_info[id].light = F1_0;
		Objects[0].id = id;
		xl_preview_dynamic_frame();
		require(xl_preview_unresolved_emitters() == 0, "ordinary weapon emitter unresolved");
		check_color(vertex, F1_0 / 4, id == CONCUSSION_ID || id == SUPER_MECH_MISS ? xl_compat::Rgb{ 1, 0.3f, 0 } : xl_compat::Rgb{ 1, 1, 1 });
		Weapon_info[id].light = previous_light;
	}
	// Real frame resolution with controlled palette and noncontiguous frame IDs
	const auto original_clip = Vclip[0];
	const auto bitmap_one = GameBitmaps[1], bitmap_three = GameBitmaps[3];
	std::array<ubyte, 768> palette;
	std::copy(gr_palette, gr_palette + palette.size(), palette.begin());
	std::fill(gr_palette, gr_palette + palette.size(), ubyte(63));
	for (unsigned c = 0; c < 3; ++c) gr_palette[c] = 0;
	auto set_color = [](int i, int r, int g, int b) { gr_palette[i * 3] = r; gr_palette[i * 3 + 1] = g; gr_palette[i * 3 + 2] = b; };
	set_color(7, 0, 0, 40);
	set_color(8, 20, 0, 0);
	set_color(9, 0, 20, 0);
	set_color(254, 10, 10, 0);
	ubyte pixels[] = { 8, 9, 0, 255 };
	for (int i : { 1, 3 }) {
		GameBitmaps[i] = {};
		GameBitmaps[i].bm_type = BM_LINEAR;
		GameBitmaps[i].bm_w = GameBitmaps[i].bm_rowsize = 4;
		GameBitmaps[i].bm_h = 1;
		GameBitmaps[i].bm_data = pixels;
	}
	GameBitmaps[1].avg_color = 7;
	Vclip[0].num_frames = 2;
	Vclip[0].frames[0].index = 1;
	Vclip[0].frames[1].index = 3;
	Vclip[0].light_value = F1_0;
	Vclip[0].play_time = 2 * F1_0;
	Objects[0].type = OBJ_FIREBALL;
	Objects[0].id = 0;
	Objects[0].lifeleft = 4 * F1_0;
	xl_preview_dynamic_frame();
	require(xl_preview_unresolved_emitters() == 0, "indexed explosion unresolved");
	check_color(vertex, F1_0 / 4, { 0.25f, 0.25f, 1 });
	// Same fallback pixels encoded as RLE_BIG: transparent bytes require runs
	ubyte encoded[] = { 13, 0, 0, 0, 7, 0, 8, 9, 0, 0xe1, 255, 0xe0, 0 };
	GameBitmaps[3].bm_flags = BM_FLAG_RLE | BM_FLAG_RLE_BIG;
	GameBitmaps[3].bm_data = encoded;
	Objects[0].lifeleft = F1_0;
	xl_preview_dynamic_frame();
	check_color(vertex, F1_0 / 8, { 0.25f, 0.25f, 1 });
	Vclip[0] = original_clip;
	GameBitmaps[1] = bitmap_one;
	GameBitmaps[3] = bitmap_three;
	std::copy(palette.begin(), palette.end(), gr_palette);
	for (int clip : { VCLIP_SMALL_EXPLOSION, VCLIP_PLAYER_HIT, VCLIP_VOLATILE_WALL_HIT }) {
		Objects[0].id = clip;
		xl_preview_dynamic_frame();
		require(xl_preview_unresolved_emitters() == 0, "retail explosion resource unresolved");
	}
	// Point-source radius is exclusive; hidden vertices must not receive it
	Objects[0].type = OBJ_LIGHT;
	Objects[0].pos.x += 64 * F1_0;
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "point light reached exclusive radius boundary");
	Objects[0].pos.x -= F1_0;
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(*std::max_element(rgb.begin(), rgb.end()) > 0, "point light missing inside radius");
	Objects[0].pos = Vertices[vertex];
	N_render_segs = 0;
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "point light reached hidden vertex");
	// Marker sources use their own segment even outside the visible set
	const int marker_vertex = Segments[0].verts[0];
	Objects[0].type = OBJ_MARKER;
	Objects[0].pos = Vertices[marker_vertex];
	Objects[0].lifeleft = 100 * F1_0 + F1_0 / 4;
	xl_preview_dynamic_frame();
	require(xl_preview_unresolved_emitters() == 0, "marker emitter unresolved");
	check_color(marker_vertex, F1_0 / 2, { 0.1f, 1, 0.1f });
	require(Objects[0].lifeleft == 100 * F1_0 + F1_0 / 4, "XL hook extended marker lifetime twice");
	Objects[0].lifeleft = 100 * F1_0 + F1_0 / 2;
	xl_preview_dynamic_frame();
	xl_preview_vertex(marker_vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "marker pulse failed to reach zero at half cycle");
	Objects[0].lifeleft = 100 * F1_0 + F1_0 / 4;
	xl_preview_dynamic_frame();
	Do_dynamic_light = 0;
	xl_preview_dynamic_frame();
	xl_preview_vertex(marker_vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "disabled lighting retained marker state");
	const auto previous_controls = Controls;
	const auto previous_frame = FrameTime;
	const auto previous_thrust = Player_ship->max_thrust;
	const auto previous_flags = Players[Player_num].flags;
	const auto previous_guided = Guided_missile[Player_num];
	const int previous_dead = Player_is_dead, previous_mode = Game_mode;
	Players[Player_num].flags = 0;
	Player_is_dead = Game_mode = 0;
	Guided_missile[Player_num] = nullptr;
	Objects[0].type = OBJ_PLAYER;
	Objects[0].id = Player_num;
	Objects[0].mtype.phys_info.flags = 0;
	Objects[0].orient = vmd_identity_matrix;
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.x += 8 * F1_0;
	Controls = {};
	Controls.forward_thrust_time = 12 * F1_0;
	FrameTime = F1_0;
	Player_ship->max_thrust = 2 * F1_0;
	read_flying_controls(&Objects[0]);
	require(Objects[0].mtype.phys_info.thrust.z == 24 * F1_0, "control fixture did not scale final thrust");
	Do_dynamic_light = 1;
	N_render_segs = Num_segments;
	xl_preview_dynamic_frame();
	require(xl_preview_unresolved_emitters() == 0, "local player light unresolved");
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	auto player_peak = *std::max_element(rgb.begin(), rgb.end());
	require(player_peak >= 28671 && player_peak <= 28672, "player light used scaled thrust instead of raw control thrust");
	Player_is_dead = 1;
	read_flying_controls(&Objects[0]);
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	player_peak = *std::max_element(rgb.begin(), rgb.end());
	require(player_peak >= 20479 && player_peak <= 20480, "dead player retained raw thrust light");
	Player_is_dead = 0;
	Players[Player_num].flags = PLAYER_FLAGS_HEADLIGHT | PLAYER_FLAGS_HEADLIGHT_ON;
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.z -= 8 * F1_0;
	xl_preview_dynamic_frame();
	require(xl_preview_unresolved_emitters() == 0, "local headlight unresolved");
	check_color(vertex, 10 * F1_0 / 8, { 1, 1, 1 });
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.x -= 4 * F1_0;
	Objects[0].pos.z -= 3 * F1_0;
	xl_preview_dynamic_frame();
	// Exact 3-4-5 triangle: fixed normalized forward dot=39321, squared branch=29490
	check_color(vertex, 29490, { 1, 1, 1 });
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.z -= 8 * F1_0;
	Objects[0].pos.z += 16 * F1_0;
	xl_preview_dynamic_frame();
	check_color(vertex, F1_0 / 8, { 1, 1, 1 });
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.x += 8 * F1_0;
	xl_preview_dynamic_frame();
	check_color(vertex, F1_0 / 8, { 1, 1, 1 });
	// Cone reaches beyond an ordinary intensity-10 point light's 640-unit radius
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.z -= 1000 * F1_0;
	xl_preview_dynamic_frame();
	check_color(vertex, 10 * F1_0 / 8, { 1, 1, 1 });
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.z -= 5120 * F1_0;
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "headlight exceeded exclusive shifted radius");
	Objects[0].pos = Vertices[vertex];
	Objects[0].pos.z -= 8 * F1_0;
	for (int flags : { int(PLAYER_FLAGS_HEADLIGHT), int(PLAYER_FLAGS_HEADLIGHT_ON) }) {
		Players[Player_num].flags = flags;
		xl_preview_dynamic_frame();
		xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
		player_peak = *std::max_element(rgb.begin(), rgb.end());
		require(player_peak >= 20479 && player_peak <= 20480, "headlight possession/on gate ignored");
	}
	xl_preview_reset_player_thrust();
	Controls = previous_controls;
	FrameTime = previous_frame;
	Player_ship->max_thrust = previous_thrust;
	Players[Player_num].flags = previous_flags;
	Guided_missile[Player_num] = previous_guided;
	Player_is_dead = previous_dead;
	Game_mode = previous_mode;
	std::array<muzzle_info, MUZZLE_QUEUE_MAX> muzzle;
	std::copy(std::begin(Muzzle_data), std::end(Muzzle_data), muzzle.begin());
	std::fill(std::begin(Muzzle_data), std::end(Muzzle_data), muzzle_info{});
	Objects[0].type = OBJ_NONE;
	Do_dynamic_light = 1;
	N_render_segs = Num_segments;
	constexpr int64_t now = 10 * F1_0;
	constexpr int duration = F1_0 / 3, scale = 3 * F1_0 / duration;
	for (int age : { 0, duration / 2, duration - 1, duration }) {
		Muzzle_data[0].pos = Vertices[vertex];
		Muzzle_data[0].segnum = 0;
		Muzzle_data[0].create_time = now - age;
		xl_preview_muzzle_snapshot(now);
		require(Muzzle_data[0].create_time == now - age, "XL snapshot mutated muzzle queue");
		xl_preview_dynamic_frame();
		xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
		const auto peak = *std::max_element(rgb.begin(), rgb.end());
		const int expected = age < duration ? (duration - age) * scale / 4 : 0;
		require(peak >= expected - 1 && peak <= expected, "muzzle decay/expiration did not reach shading");
	}
	// Exercise the generated hook and stock-owned expiry, not only the snapshot API
	Muzzle_data[0].create_time = timer_query() - duration - 1024;
	set_dynamic_light();
	require(Muzzle_data[0].create_time == 0, "stock muzzle expiration did not run");
	xl_preview_dynamic_frame();
	xl_preview_vertex(vertex, 0, &rgb[0], &rgb[1], &rgb[2]);
	require(rgb == std::array<int32_t, 3>{}, "expired stock muzzle persisted in XL shading");
	std::fill(std::begin(Muzzle_data), std::end(Muzzle_data), muzzle_info{});
	xl_preview_muzzle_snapshot(now);
	std::copy(muzzle.begin(), muzzle.end(), Muzzle_data);
	for (int i = 0; i <= Highest_object_index; ++i) Objects[i].type = types[i];
	Objects[0] = first;
	std::copy(render_list.begin(), render_list.end(), Render_list);
	N_render_segs = previous_count;
	Do_dynamic_light = previous_enabled;
}
std::vector<int32_t> face_lights()
{
	std::vector<int32_t> values;
	for (int seg = 0; seg < Num_segments; ++seg)
		for (int side = 0; side < 6; ++side)
			for (int corner = 0; corner < 4; ++corner)
				values.push_back(Segments[seg].sides[side].uvls[corner].l);
	return values;
}
void changing_lights(const xl_compat::Inventory &source, const char *filename = "xlprobe.rl2", unsigned expected_count = 70)
{
	unsigned checked = 0, changed = 0, color_changes = 0;
	for (const auto &light : source.light_indices) {
		require(load_level(filename) == 0, "light fixture reload failed");
		std::fill(Light_subtracted, Light_subtracted + Num_segments, 0);
		auto expected = face_lights();
		const auto initial = expected;
		// Pinned XL light.cpp ChangeLight legacy mode: ordered, clamped UVL deltas
		auto apply_expected = [&](int direction) {
			for (const auto &index : source.light_indices) {
				if (index[0] != light[0] || index[1] != light[1]) continue;
				for (int i = index[3]; i < index[3] + index[2]; ++i) {
					const auto &delta = source.delta_lights.at(i);
					for (int k = 0; k < 4; ++k) {
						auto &value = expected.at(delta[0] * 24 + delta[1] * 4 + k);
						value = (std::max) (0, value + direction * delta[3 + k] * 2048);
					}
				}
			}
		};
		apply_expected(-1);
		require(subtract_light(light[0], light[1]) == 1, "first subtraction ignored");
		require(face_lights() == expected, "pilot light-off UVLs differ from reference formula");
		require(subtract_light(light[0], light[1]) == 0 && face_lights() == expected, "repeated light-off changed state");
		if (expected != initial) ++changed;
		for (int seg = 0; seg < Num_segments; ++seg) {
			for (int side = 0; side < 6; ++side) {
				int vertices[4];
				get_side_verts(vertices, seg, side);
				for (int corner = 0; corner < 4; ++corner) {
					const auto offset = seg * 24 + side * 4 + corner;
					if (initial[offset] == expected[offset]) continue;
					std::array<int32_t, 3> before{}, after{};
					xl_preview_vertex(vertices[corner], initial[offset], &before[0], &before[1], &before[2]);
					xl_preview_vertex(vertices[corner], Segments[seg].sides[side].uvls[corner].l, &after[0], &after[1], &after[2]);
					if (before != after) ++color_changes;
				}
			}
		}
		apply_expected(1);
		require(add_light(light[0], light[1]) == 1, "first addition ignored");
		require(face_lights() == expected, "pilot light-on UVLs differ from reference formula");
		require(add_light(light[0], light[1]) == 0 && face_lights() == expected, "repeated light-on changed state");
		++checked;
	}
	require(checked == expected_count && changed > 0 && color_changes > 0, "pilot changing-light coverage missing");
	std::cout << "XL pilot light updates: " << checked << " sources checked, " << changed << " changed UVLs, " << color_changes << " colored corners changed; legacy scalar path only\n";
	require(load_level(filename) == 0, "final light fixture reload failed");
	std::fill(Light_subtracted, Light_subtracted + Num_segments, 0);
}
void write_bytes(const char *path, const xl_compat::Bytes &bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
	out.close();
	require(bool(out), "cannot write private fixture");
}
void original_mission_save(const xl_compat::Inventory &source, int vertex)
{
	char error[256] = {};
	require(load_base_mission("d2-xl", error, sizeof(error)), error);
	require(!std::strcmp(Current_mission_longname, "D2-XL: Counterstrike!"), "outer mission descriptor lost precedence");
	require(Last_level == 24 && Last_secret_level == -6, "original mission list changed");
	std::strcpy(Players[Player_num].callsign, "XLTest");
	Difficulty_level = 2;
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	object_dependencies();
	require(Current_level_num == 1 && !std::strcmp(Gamesave_current_filename, "d2leva-1.rl2"), "original mission startup failed");
	auto robot_count = [] { int count = 0; for (int i = 0; i <= Highest_object_index; ++i) if (Objects[i].type == OBJ_ROBOT) ++count; return count; };
	require(robot_count() == 38, "normal mission startup removed robots");
	require(!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "normal mission startup granted invulnerability");
	const auto &light = source.light_indices.front();
	require(subtract_light(light[0], light[1]) == 1, "save fixture light did not switch off");
	Walls[6].cloak_value = 11;
	const auto expected_lights = face_lights();
	const auto expected_energy = Players[Player_num].energy;
	const auto expected_flags = Players[Player_num].flags;
	char save[] = "xl-pilot-state.sav", description[] = "XL extension persistence";
	// The native low-level save consumes its caller's pause
	stop_time();
	require(state_save_all_sub(save, description) != 0, "original mission save failed");
	Walls[6].cloak_value = 30;
	add_light(light[0], light[1]);
	Players[Player_num].energy = 1;
	require(state_restore_all_sub(save, 0) != 0, "original mission restore failed");
	require(!std::strcmp(Current_mission_filename, "d2-xl") && Current_level_num == 1, "restore selected wrong mission/level");
	require(!std::strcmp(Gamesave_current_filename, "d2leva-1.rl2"), "restore lost original level identity");
	require(Walls[6].cloak_value == 11, "restore lost live wall opacity");
	require(face_lights() == expected_lights, "restore lost switched-light UVLs");
	require(Players[Player_num].energy == expected_energy && Players[Player_num].flags == expected_flags, "restore lost player state");
	require(robot_count() == 38, "restore lost live robots");
	xl_preview_begin_face(Walls[6].segnum, Walls[6].sidenum);
	require(xl_preview_alpha(1) == 20.0f / 31.0f, "restored opacity did not reach material adapter");
	xl_preview_end_face();
	colored(source, vertex);
	std::cout << "Original mission save: identity, 38 robots, opacity, changed lights, player state and XL activation restored; progression unverified\n";
}
void normal_input_frames()
{
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(!cheats.enabled && !cheats.ghostphysics && !cheats.monsterdamage && !cheats.robotfiringsuspended,
	        "normal input probe started with combat cheats");
	require(!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "normal input probe started invulnerable");
	const auto start_position = ConsoleObject->pos;
	const auto start_energy = Players[Player_num].energy;
	const auto start_shields = Players[Player_num].shields;
	const auto start_time = GameTime64;
	std::set<int> player_shots, robot_shots;
	int remainder = 0, frames = 0;
	for (; frames < 600 && !Player_is_dead; ++frames) {
		remainder += F1_0;
		FrameTime = remainder / 60;
		remainder %= 60;
		std::memset(&Controls, 0, sizeof(Controls));
		Controls.forward_thrust_time = FrameTime;
		Controls.fire_primary_state = 1;
		calc_game_time();
		GameProcessFrame();
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON) {
				const auto &o = Objects[i];
				if (o.ctype.laser_info.parent_type == OBJ_PLAYER) player_shots.insert(o.signature);
				if (o.ctype.laser_info.parent_type == OBJ_ROBOT) robot_shots.insert(o.signature);
			}
	}
	std::memset(&Controls, 0, sizeof(Controls));
	require(GameTime64 > start_time, "normal simulation did not advance");
	require(!player_shots.empty() && Players[Player_num].energy < start_energy, "ordinary primary fire did not create charged shots");
	require(!robot_shots.empty(), "normal pilot encounter did not exercise robot firing");
	require(Players[Player_num].shields < start_shields, "normal pilot encounter did not exercise player damage");
	require(vm_vec_dist(&start_position, &ConsoleObject->pos) > F1_0, "ordinary thrust did not move the player");
	require(!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "normal input probe acquired unexplained invulnerability");
	std::cout << "Normal input simulation: frames=" << frames << " player_shots=" << player_shots.size()
	          << " robot_shots=" << robot_shots.size() << " shields=" << Players[Player_num].shields
	          << " dead=" << Player_is_dead << " segment=" << ConsoleObject->segnum
	          << "; fixed forward/fire inputs only, progression unverified\n";
}
std::vector<unsigned char> gameplay_definitions()
{
	std::vector<unsigned char> result;
	auto append = [&](const auto *data, int count) {
		const auto *bytes = reinterpret_cast<const unsigned char *>(data);
		result.insert(result.end(), bytes, bytes + sizeof(*data) * count);
	};
	append(&N_robot_types, 1);
	append(Robot_info, N_robot_types);
	append(&N_weapon_types, 1);
	append(Weapon_info, N_weapon_types);
	append(&N_powerup_types, 1);
	append(Powerup_info, N_powerup_types);
	append(&N_robot_joints, 1);
	append(Robot_joints, N_robot_joints);
	return result;
}
uint64_t route_activated_triggers = 0, route_passable_targets = 0;
#include "generator_route_test.h"
void observe_route_triggers()
{
	const auto activated = xl_preview_trigger_mask();
	route_activated_triggers |= activated;
	for (int t = 0; t < Num_triggers; ++t) {
		// Generator links identify segments, not wall surfaces
		if (!(activated & (uint64_t(1) << t)) || Triggers[t].type == TT_EXIT || Triggers[t].type == TT_MATCEN) continue;
		bool passable = Triggers[t].num_links > 0;
		for (int link = 0; link < Triggers[t].num_links; ++link) {
			const int seg = Triggers[t].seg[link], side = Triggers[t].side[link];
			const auto &wall = Walls[Segments[seg].sides[side].wall_num];
			const int doorway = WALL_IS_DOORWAY(&Segments[seg], side);
			static int previous[MAX_TRIGGERS][MAX_WALLS_PER_LINK]{};
			const int state = 1 + wall.type + 16 * wall.state + 256 * wall.flags + 65536 * doorway;
			if (previous[t][link] != state) {
				std::cout << "Trigger target: trigger=" << t << " segment=" << seg << " side=" << side
				          << " type=" << int(wall.type) << " state=" << int(wall.state) << " flags=" << int(wall.flags) << " doorway=" << doorway << '\n';
				previous[t][link] = state;
			}
			passable = passable && (doorway & WID_FLY_FLAG);
		}
		if (passable) route_passable_targets |= uint64_t(1) << t;
	}
}

void verify_branch_checkpoint(int branch)
{
	auto target_state = [&] {
		std::vector<int> values{ Triggers[branch].flags };
		for (int link = 0; link < Triggers[branch].num_links; ++link) {
			const int seg = Triggers[branch].seg[link], side = Triggers[branch].side[link];
			const auto &surface = Segments[seg].sides[side];
			const auto &wall = Walls[surface.wall_num];
			values.insert(values.end(), { surface.wall_num, surface.tmap_num, surface.tmap_num2, wall.type, wall.flags,
			                              wall.state, wall.hps, wall.cloak_value, WALL_IS_DOORWAY(&Segments[seg], side) });
		}
		return values;
	};
	const auto before = target_state();
	const auto shields = Players[Player_num].shields, energy = Players[Player_num].energy;
	char checkpoint[] = "xl-normal-branch.sav", description[] = "Normal authored branch";
	// The native low-level save consumes its caller's pause
	stop_time();
	require(state_save_all_sub(checkpoint, description) != 0, "branch checkpoint save failed");
	require(state_restore_all_sub(checkpoint, 0) != 0, "branch checkpoint restore failed");
	require(target_state() == before && Players[Player_num].shields == shields && Players[Player_num].energy == energy,
	        "branch restore changed target walls or earned player resources");
	require(xl_preview_trigger_mask() == 0, "diagnostic activation mask leaked across load");
	std::cout << "Normal branch persistence: trigger=" << branch << " targets and earned resources restored\n";
}

void navigate_objective(bool reactor, bool escape = false, int branch = -1, int visit = -1, int cross_from = -1, bool campaign = false, int second_level = 0)
{
	require(Num_triggers == (second_level ? 39 : 13) && branch >= -1 && branch < Num_triggers, "unexpected pilot trigger scope");
	const int key_id = second_level == 1 ? POW_KEY_BLUE : POW_KEY_RED;
	const int key_flag = second_level == 1 ? KEY_BLUE : KEY_RED;
	require(visit >= -1 && visit <= Highest_segment_index, "invalid visit objective");
	require(cross_from >= -1 && cross_from <= Highest_segment_index, "invalid crossing origin");
	const bool exploring = branch >= 0 || visit >= 0;
	if (branch >= 0 && (route_activated_triggers & route_passable_targets & (uint64_t(1) << branch))) {
		require(!Player_is_dead, "prior branch coverage ended dead");
		std::cout << "Normal branch navigation: trigger=" << branch << " frames=0 dead=0 activated=1 targets_passable=1; exercised on key route\n";
		verify_branch_checkpoint(branch);
		return;
	}
	if (reactor) {
		const auto shields = Players[Player_num].shields, energy = Players[Player_num].energy;
		char key_checkpoint[] = "xl-normal-key.sav", reactor_checkpoint[] = "xl-normal-reactor.sav", branch_checkpoint[] = "xl-normal-branch.sav";
		const bool after_reactor = escape || (visit >= 0 && Control_center_destroyed);
		char *checkpoint = cross_from >= 0 ? branch_checkpoint : after_reactor ? reactor_checkpoint
		                                                                       : key_checkpoint;
		const auto countdown = Countdown_timer;
		require(state_restore_all_sub(checkpoint, 0) != 0, "normal key checkpoint restore failed");
		require((Players[Player_num].flags & KEY_RED) && (Players[Player_num].shields == shields && Players[Player_num].energy == energy),
		        "normal key checkpoint changed earned inventory");
		if (after_reactor) require(Control_center_destroyed && Countdown_timer == countdown && countdown > 0, "earned reactor countdown changed on restore");
	} else if (!second_level) {
		route_activated_triggers = route_passable_targets = 0;
		d_srand_stream(D_RNG_SIM, 1);
		d_srand_stream(D_RNG_FX, 1);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		xl_checkpoint_capture_sources("d2leva-1.rl2");
	} else {
		require(Current_level_num == 2 && !std::strcmp(Gamesave_current_filename, "d2leva-2.rl2"), "second-level route requires earned arrival");
		route_activated_triggers = route_passable_targets = 0;
	}
	int key = -1;
	int robots = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (reactor ? Objects[i].type == OBJ_CNTRLCEN : (Objects[i].type == OBJ_POWERUP && Objects[i].id == key_id)) key = i;
		else if (Objects[i].type == OBJ_ROBOT) ++robots;
	require((reactor || second_level == 2 || robots == (second_level ? 69 : 38)) && Difficulty_level == 2 && !cheats.enabled && !cheats.monsterdamage && !cheats.ghostphysics && !cheats.robotfiringsuspended,
	        "normal navigation fixture has altered combat rules");
	require(!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "normal navigation fixture is invulnerable");
	require(xl_preview_pilot_active() && !xl_preview_pilot_completed(), "navigation did not start with fresh pilot admission");
	int branch_wall = -1;
	if (branch >= 0) {
		require(branch < Num_triggers && Triggers[branch].type != TT_EXIT, "invalid branch objective");
		for (int i = 0; i < Num_walls; ++i)
			if (wall_trigger_index(Walls[i].trigger) == branch) branch_wall = i;
		require(branch_wall >= 0, "branch has no authored host wall");
	}
	int exit_wall = -1;
	if (escape)
		for (int i = 0; i < Num_walls; ++i)
			if (wall_trigger_index(Walls[i].trigger) >= 0 && wall_trigger_index(Walls[i].trigger) < Num_triggers && Triggers[wall_trigger_index(Walls[i].trigger)].type == TT_EXIT) exit_wall = i;
	require(escape ? exit_wall >= 0 : key >= 0, "navigation fixture has no objective");
	if (escape && !campaign) {
		const int paused = game_is_time_paused();
		require(input_demo_start_quick_recording_common() != 0, "cannot record earned escape checkpoint");
		require(game_is_time_paused() == paused, "checkpoint recording changed the caller's pause state");
	}
	bool crossed = false, approached = cross_from < 0;
	auto complete = [&] { return cross_from >= 0 ? crossed : visit >= 0 ? ConsoleObject->segnum == visit
		                                                 : branch >= 0  ? bool(xl_preview_trigger_mask() & route_passable_targets & (uint64_t(1) << branch))
		                                                 : escape       ? (campaign ? Current_level_num == 2 : bool(xl_preview_pilot_completed()))
		                                                 : reactor      ? bool(Control_center_destroyed)
		                                                                : bool(Players[Player_num].flags & key_flag); };
	int remainder = 0, frames = 0, last_segment = -1;
	const int entry_lives = Players[Player_num].lives;
	GeneratorRouteObservation generators;
	fix transition_countdown = 0;
	for (; frames < (exploring ? 36000 : 10800) && !Player_is_dead && !complete(); ++frames) {
		const int start = ConsoleObject->segnum;
		if (start == cross_from) approached = true;
		// Keep the last naturally reached state before crossing the actual exit
		if (escape && start == Walls[exit_wall].segnum) {
			char checkpoint[] = "xl-normal-exit-approach.sav", description[] = "Earned exit approach";
			// The native low-level save consumes its caller's pause
			stop_time();
			require(state_save_all_sub(checkpoint, description) != 0, "cannot save earned exit approach");
		}
		int goal = !approached ? cross_from : visit >= 0 ? visit
		                                  : branch >= 0  ? Walls[branch_wall].segnum
		                                  : escape       ? Walls[exit_wall].segnum
		                                                 : Objects[key].segnum,
		    pickup = key, switch_wall = -1;
		std::vector<int> parent(Highest_segment_index + 1, -1), queue{ start };
		std::vector<int> steps(Highest_segment_index + 1, -1);
		parent[start] = start;
		steps[start] = 0;
		for (std::size_t q = 0; q < queue.size(); ++q) {
			const int seg = queue[q];
			for (int side = 0; side < 6; ++side) {
				const int child = Segments[seg].children[side];
				if (child < 0 || parent[child] >= 0) continue;
				// The reactor/wreck obstructs the narrow center route; use the side loop
				// for exploration rather than treating a flyable portal as a clear corridor
				if (visit >= 0 && child != visit && Objects[key].type == OBJ_CNTRLCEN && child == Objects[key].segnum) continue;
				bool pass = WALL_IS_DOORWAY(&Segments[seg], side) & WID_FLY_FLAG;
				const int wall = Segments[seg].sides[side].wall_num;
				if (!pass && wall >= 0) {
					const auto &w = Walls[wall];
					pass = w.type == WALL_BLASTABLE || (w.type == WALL_DOOR && !(w.flags & WALL_DOOR_LOCKED) &&
					                                    (w.keys == KEY_NONE || (w.keys & Players[Player_num].flags)));
				}
				if (pass) {
					parent[child] = seg;
					steps[child] = steps[seg] + 1;
					queue.push_back(child);
				}
			}
		}
		int pickup_steps = escape ? 0 : exploring                                                                                                                   ? 4
		                            : (reactor || second_level) && (Players[Player_num].shields < i2f(50) || (branch >= 0 && Players[Player_num].energy < i2f(50))) ? Highest_segment_index + 1
		                                                                                                                                                            : 6;
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &o = Objects[i];
			if (o.type != OBJ_POWERUP || o.segnum < 0 || steps[o.segnum] < 0 || steps[o.segnum] >= pickup_steps) continue;
			if ((o.id == POW_SHIELD_BOOST && Players[Player_num].shields < i2f(90)) ||
			    (exploring && o.id == POW_ENERGY && Players[Player_num].energy < i2f(80)) ||
			    (o.id == POW_LASER && Players[Player_num].laser_level < MAX_LASER_LEVEL)) {
				pickup = i;
				goal = o.segnum;
				pickup_steps = steps[goal];
			}
		}
		auto switch_approach = [&](int wall) {
			const auto &host = Walls[wall];
			if (exploring && host.type != WALL_OVERLAY && !(WALL_IS_DOORWAY(&Segments[host.segnum], host.sidenum) & WID_FLY_FLAG)) return -1;
			if (parent[host.segnum] >= 0) return host.segnum;
			if (!exploring) return -1;
			vms_vector target;
			compute_center_point_on_side(&target, &Segments[host.segnum], host.sidenum);
			int best = -1;
			for (const int candidate : queue) {
				if (best >= 0 && steps[candidate] >= steps[best]) continue;
				vms_vector origin;
				compute_segment_center(&origin, &Segments[candidate]);
				if (vm_vec_dist(&origin, &target) > i2f(100)) continue;
				fvi_query query{};
				query.p0 = &origin;
				query.p1 = &target;
				query.startseg = candidate;
				query.thisobjnum = Players[Player_num].objnum;
				query.flags = FQ_TRANSWALL;
				fvi_info hit{};
				const int result = find_vector_intersection(&query, &hit);
				if (result == HIT_NONE || (result == HIT_WALL && hit.hit_side_seg == host.segnum && hit.hit_side == host.sidenum)) best = candidate;
			}
			return best;
		};
		if (branch >= 0 && pickup == key) {
			int objective_wall = branch_wall;
			const auto &host = Walls[branch_wall];
			if (host.type != WALL_OVERLAY && !(WALL_IS_DOORWAY(&Segments[host.segnum], host.sidenum) & WID_FLY_FLAG)) {
				for (int wall = 0; wall < Num_walls; ++wall)
					if (wall_trigger_index(host.controlling_trigger) >= 0 && wall_trigger_index(Walls[wall].trigger) == wall_trigger_index(host.controlling_trigger)) objective_wall = wall;
			}
			const int approach = switch_approach(objective_wall);
			if (approach >= 0) {
				switch_wall = objective_wall;
				goal = approach;
			}
		}
		if (parent[goal] < 0) {
			int best = Highest_segment_index + 1;
			for (int wall = 0; wall < Num_walls; ++wall) {
				const auto &host = Walls[wall];
				if (wall_trigger_index(host.trigger) < 0 || wall_trigger_index(host.trigger) >= Num_triggers) continue;
				const int approach = switch_approach(wall);
				if (approach < 0 || steps[approach] >= best) continue;
				const auto &trigger = Triggers[wall_trigger_index(host.trigger)];
				if ((trigger.flags & TF_DISABLED) || (trigger.type != TT_OPEN_DOOR && trigger.type != TT_OPEN_WALL && trigger.type != TT_UNLOCK_DOOR)) continue;
				bool opens_frontier = false;
				for (int link = 0; link < trigger.num_links; ++link) {
					const int seg = trigger.seg[link], side = trigger.side[link];
					const int child = Segments[seg].children[side];
					if (child >= 0 && ((parent[seg] >= 0) != (parent[child] >= 0))) opens_frontier = true;
				}
				if (opens_frontier) {
					switch_wall = wall;
					best = steps[approach];
				}
			}
			require(switch_wall >= 0, "objective requires a prerequisite not yet handled by input navigator");
			goal = switch_approach(switch_wall);
		}
		int next = goal;
		while (parent[next] != start && next != start) next = parent[next];
		int blocking_wall = -1;
		vms_vector target{};
		if (escape) compute_center_point_on_side(&target, &Segments[goal], Walls[exit_wall].sidenum);
		else if (visit >= 0 && pickup == key) compute_segment_center(&target, &Segments[goal]);
		else target = Objects[pickup].pos;
		if (switch_wall >= 0) {
			const auto &host = Walls[switch_wall];
			const int child = Segments[host.segnum].children[host.sidenum];
			if (child >= 0 && (WALL_IS_DOORWAY(&Segments[host.segnum], host.sidenum) & WID_FLY_FLAG))
				compute_segment_center(&target, &Segments[child]);
			else {
				compute_center_point_on_side(&target, &Segments[host.segnum], host.sidenum);
				// Shoot different parts of the authored switch face; its center may be transparent
				if (exploring && start == goal) {
					int verts[4];
					get_side_verts(verts, host.segnum, host.sidenum);
					const int sample = (frames / 90) % 25;
					const fix u = F1_0 * (1 + 2 * (sample % 5)) / 10;
					const fix v = F1_0 * (1 + 2 * (sample / 5)) / 10;
					vms_vector bottom, top, edge;
					vm_vec_sub(&edge, &Vertices[verts[1]], &Vertices[verts[0]]);
					vm_vec_scale_add(&bottom, &Vertices[verts[0]], &edge, u);
					vm_vec_sub(&edge, &Vertices[verts[2]], &Vertices[verts[3]]);
					vm_vec_scale_add(&top, &Vertices[verts[3]], &edge, u);
					vm_vec_sub(&edge, &top, &bottom);
					vm_vec_scale_add(&target, &bottom, &edge, v);
				}
			}
		}
		if (start != goal) {
			int crossing = -1;
			for (int side = 0; side < 6; ++side)
				if (Segments[start].children[side] == next) crossing = side;
			require(crossing >= 0, "navigation path lost adjacent portal");
			vms_vector portal, inward, center;
			compute_center_point_on_side(&portal, &Segments[start], crossing);
			const int portal_wall = Segments[start].sides[crossing].wall_num;
			if (exploring && portal_wall >= 0 && Walls[portal_wall].type == WALL_BLASTABLE &&
			    !(WALL_IS_DOORWAY(&Segments[start], crossing) & WID_FLY_FLAG)) blocking_wall = portal_wall;
			compute_segment_center(&center, &Segments[next]);
			vm_vec_sub(&inward, &center, &portal);
			const fix depth = vm_vec_normalize(&inward);
			vm_vec_scale_add(&target, &portal, &inward, (std::min) (depth, ConsoleObject->size * 2));
			object probe = *ConsoleObject;
			probe.segnum = next;
			probe.pos = target;
			if (object_intersects_wall(&probe)) {
				vms_vector offset;
				vm_vec_sub(&offset, &center, &target);
				for (int sample = 1; sample <= 16; ++sample) {
					vm_vec_scale_add(&probe.pos, &target, &offset, sample * (F1_0 / 16));
					if (find_point_seg(&probe.pos, next) == next && !object_intersects_wall(&probe)) {
						target = probe.pos;
						break;
					}
				}
			}
			fvi_query query{};
			query.p0 = &ConsoleObject->pos;
			query.p1 = &target;
			query.startseg = start;
			query.rad = ConsoleObject->size;
			query.thisobjnum = Players[Player_num].objnum;
			fvi_info hit{};
			if (find_vector_intersection(&query, &hit) == HIT_WALL && (hit.hit_side_seg != start || hit.hit_side != crossing))
				compute_segment_center(&target, &Segments[next]);
			if (blocking_wall >= 0) {
				target = portal;
				// Damaged blastable textures have holes; aim at several remaining patches
				const int sample = (frames / 180) % 5;
				if (sample) {
					int verts[4];
					get_side_verts(verts, start, crossing);
					vms_vector offset;
					vm_vec_sub(&offset, &Vertices[verts[sample - 1]], &target);
					vm_vec_scale_add2(&target, &offset, F1_0 / 2);
				}
			}
		}
		vms_vector direction;
		vm_vec_sub(&direction, &target, &ConsoleObject->pos);
		const fix distance = vm_vec_normalize(&direction);
		vms_vector aim = direction;
		fix nearest = i2f(100);
		bool targeting_thief = false;
		for (int i = 0; !escape && i <= Highest_object_index; ++i) {
			if (Objects[i].type == OBJ_ROBOT) {
				if (Robot_info[Objects[i].id].companion) continue;
			} else if (!reactor || branch >= 0 || Control_center_destroyed || Objects[i].type != OBJ_CNTRLCEN) continue;
			vms_vector delta;
			vm_vec_sub(&delta, &Objects[i].pos, &ConsoleObject->pos);
			const fix range = vm_vec_normalize(&delta);
			const bool thief = Objects[i].type == OBJ_ROBOT && Robot_info[Objects[i].id].thief;
			// Do not turn away from an armed defender to chase the closer thief
			if (second_level) {
				if (range >= i2f(100) || (nearest < i2f(100) && !targeting_thief && thief) ||
				    (thief == targeting_thief && range >= nearest)) continue;
			} else if (range >= nearest) continue;
			fvi_query query{};
			query.p0 = &ConsoleObject->pos;
			query.p1 = &Objects[i].pos;
			query.startseg = start;
			query.thisobjnum = Players[Player_num].objnum;
			fvi_info hit{};
			if (find_vector_intersection(&query, &hit) == HIT_NONE) {
				vms_vector predicted = Objects[i].pos;
				const fix flight = fixdiv(range, Weapon_info[LASER_ID_L1].speed[Difficulty_level]);
				if (Objects[i].movement_type == MT_PHYSICS) vm_vec_scale_add2(&predicted, &Objects[i].mtype.phys_info.velocity, flight);
				vm_vec_sub(&aim, &predicted, &ConsoleObject->pos);
				vm_vec_normalize(&aim);
				nearest = range;
				targeting_thief = thief;
			}
		}
		remainder += F1_0;
		FrameTime = remainder / 60;
		remainder %= 60;
		std::memset(&Controls, 0, sizeof(Controls));
		const auto &orientation = ConsoleObject->orient;
		auto dot = [](const vms_vector &a, const vms_vector &b) { return double(vm_vec_dot(&a, &b)) / F1_0; };
		auto input = [&](double value) { return fix(std::clamp(value, -1.0, 1.0) * FrameTime); };
		const double x = dot(aim, orientation.rvec), y = dot(aim, orientation.uvec), z = dot(aim, orientation.fvec);
		Controls.heading_time = input(std::atan2(x, z) * 8 - double(ConsoleObject->mtype.phys_info.rotvel.y) / F1_0 * 1.5);
		Controls.pitch_time = input(-std::atan2(y, std::hypot(x, z)) * 8 - double(ConsoleObject->mtype.phys_info.rotvel.x) / F1_0 * 1.5);
		const bool fighting = nearest < i2f(100);
		const bool shoot_switch = exploring && switch_wall >= 0 && start == goal &&
		                          !(WALL_IS_DOORWAY(&Segments[Walls[switch_wall].segnum], Walls[switch_wall].sidenum) & WID_FLY_FLAG);
		double approach = (std::max) (0.0, double(distance) / F1_0 - (shoot_switch ? 20.0 : blocking_wall >= 0 ? 8.0
		                                                                                                       : 0.0));
		if (shoot_switch && Walls[switch_wall].segnum != start) {
			// The visibility query used this center, not the portal where we entered
			vms_vector firing_position;
			compute_segment_center(&firing_position, &Segments[start]);
			vm_vec_sub(&direction, &firing_position, &ConsoleObject->pos);
			approach = double(vm_vec_normalize(&direction)) / F1_0;
		}
		// Keep moving through a narrow portal while engaging a distant defender
		const double speed = fighting ? (cross_from >= 0 && nearest > i2f(60) ? (std::min) (15.0, approach * 2) : 0.0) : (std::min) (25.0, approach * 2);
		const auto &velocity = ConsoleObject->mtype.phys_info.velocity;
		Controls.forward_thrust_time = input((dot(direction, orientation.fvec) * speed - dot(velocity, orientation.fvec)) / 12);
		const double dodge = fighting ? (frames / 90 % 2 ? 20.0 : -20.0) : 0.0;
		Controls.sideways_thrust_time = input((dot(direction, orientation.rvec) * speed + dodge - dot(velocity, orientation.rvec)) / 12);
		Controls.vertical_thrust_time = input((dot(direction, orientation.uvec) * speed - dot(velocity, orientation.uvec)) / 12);
		bool barrier = blocking_wall >= 0;
		if (!fighting) {
			fvi_query query{};
			query.p0 = &ConsoleObject->pos;
			query.p1 = &target;
			query.startseg = start;
			query.thisobjnum = Players[Player_num].objnum;
			fvi_info hit{};
			if (find_vector_intersection(&query, &hit) == HIT_WALL && hit.hit_side_seg >= 0 && hit.hit_side >= 0) {
				const int wall = Segments[hit.hit_side_seg].sides[hit.hit_side].wall_num;
				barrier = !exploring || (wall >= 0 && (Walls[wall].type == WALL_DOOR || Walls[wall].type == WALL_BLASTABLE));
			}
		}
		bool switch_visible = true;
		if (shoot_switch) {
			fvi_query query{};
			query.p0 = &ConsoleObject->pos;
			query.p1 = &target;
			query.startseg = start;
			query.thisobjnum = Players[Player_num].objnum;
			query.flags = FQ_TRANSPOINT;
			fvi_info hit{};
			const int result = find_vector_intersection(&query, &hit);
			switch_visible = result == HIT_NONE || (result == HIT_WALL && hit.hit_side_seg == Walls[switch_wall].segnum && hit.hit_side == Walls[switch_wall].sidenum);
		}
		Controls.fire_primary_state = (fighting || barrier || (switch_wall >= 0 && start == goal)) && z > (nearest < i2f(40) ? 0.995 : 0.999) && switch_visible;
		Controls.fire_secondary_state = fighting && z > 0.999 && nearest > i2f(20);
		if (second_level && Controls.fire_secondary_state) {
			// Avoid firing an explosive missile into a nearby corridor wall
			vms_vector clearance;
			vm_vec_scale_add(&clearance, &ConsoleObject->pos, &orientation.fvec, i2f(40));
			fvi_query query{};
			query.p0 = &ConsoleObject->pos;
			query.p1 = &clearance;
			query.rad = F1_0;
			query.startseg = start;
			query.thisobjnum = Players[Player_num].objnum;
			fvi_info hit{};
			if (find_vector_intersection(&query, &hit) != HIT_NONE) Controls.fire_secondary_state = 0;
		}
		if (escape && !campaign) input_demo_record_input_sample();
		const fix shields_before_frame = Players[Player_num].shields;
		do_weapon_n_item_stuff();
		calc_game_time();
		if (campaign) transition_countdown = Countdown_timer;
		GameProcessFrame();
		if (second_level && Players[Player_num].shields < shields_before_frame) {
			std::cout << "Level two route damage: frame=" << frames << " segment=" << ConsoleObject->segnum << " shields=" << Players[Player_num].shields
			          << " enemy_range=" << double(nearest) / F1_0 << " aim_dot=" << z << " primary=" << int(Controls.fire_primary_state)
			          << " secondary=" << int(Controls.fire_secondary_state) << '\n';
			if (second_level == 2) {
				std::cout << "Red route evasion: sideways_speed=" << dot(ConsoleObject->mtype.phys_info.velocity, ConsoleObject->orient.rvec)
				          << " requested=" << dodge;
				for (int axis = 0; axis < 4; ++axis) {
					vms_vector endpoint;
					vm_vec_scale_add(&endpoint, &ConsoleObject->pos, axis < 2 ? &ConsoleObject->orient.rvec : &ConsoleObject->orient.uvec,
					                 (axis % 2 ? -1 : 1) * i2f(20));
					fvi_query query{};
					query.p0 = &ConsoleObject->pos;
					query.p1 = &endpoint;
					query.startseg = ConsoleObject->segnum;
					query.rad = ConsoleObject->size;
					query.thisobjnum = Players[Player_num].objnum;
					fvi_info hit{};
					const int result = find_vector_intersection(&query, &hit);
					std::cout << " clearance" << axis << '=' << (result == HIT_NONE ? 20.0 : double(vm_vec_dist(&ConsoleObject->pos, &hit.hit_pnt)) / F1_0);
				}
				std::cout << '\n';
			}
		}
		if (campaign && complete()) {
			++frames;
			break;
		}
		if (cross_from >= 0 && Players[Player_num].shields < shields_before_frame)
			std::cout << "Portal damage: frame=" << frames << " segment=" << ConsoleObject->segnum << " shields=" << double(Players[Player_num].shields) / F1_0
			          << " enemy_range=" << double(nearest) / F1_0 << " primary=" << int(Controls.fire_primary_state) << " secondary=" << int(Controls.fire_secondary_state) << '\n';
		observe_route_triggers();
		if (second_level) generators.observe();
		if (cross_from >= 0 && start == cross_from && ConsoleObject->segnum == visit) crossed = true;
		if (cross_from >= 0 && !crossed && ConsoleObject->segnum == visit) approached = false;
		if (visit >= 0) require(Players[Player_num].lives == entry_lives && !Reactor_countdown_paused,
			                    "secret entry lost a life or paused the native countdown");
		if (ConsoleObject->segnum != last_segment || frames % 600 == 0) {
			if (exploring && frames % 600 == 0 && start != next) {
				for (int side = 0; side < 6; ++side) {
					if (Segments[start].children[side] != next) continue;
					const int wall = Segments[start].sides[side].wall_num;
					std::cout << "Navigation portal: segment=" << start << " side=" << side << " next=" << next
					          << " doorway=" << WALL_IS_DOORWAY(&Segments[start], side) << " wall=" << wall;
					if (wall >= 0) std::cout << " type=" << int(Walls[wall].type) << " flags=" << int(Walls[wall].flags) << " hp=" << Walls[wall].hps;
					std::cout << " pos=" << ConsoleObject->pos.x << "," << ConsoleObject->pos.y << "," << ConsoleObject->pos.z
					          << " target=" << target.x << "," << target.y << "," << target.z << " radius=" << ConsoleObject->size << '\n';
				}
			}
			last_segment = ConsoleObject->segnum;
			std::cout << "Normal navigation frame=" << frames << " segment=" << last_segment << " next=" << next
			          << " shields=" << Players[Player_num].shields << " energy=" << Players[Player_num].energy
			          << " goal=" << goal << " switch=" << switch_wall << " switch_visible=" << switch_visible
			          << " aim_dot=" << z << " enemy_range=" << double(nearest) / F1_0 << " kills=" << Players[Player_num].num_kills_level << '\n';
			if (second_level) std::cout << "Route weapons: primary=" << int(Players[Player_num].primary_weapon) << " owned=" << Players[Player_num].primary_weapon_flags << '\n';
		}
	}
	std::memset(&Controls, 0, sizeof(Controls));
	if (second_level) {
		std::cout << "Level two generator observations: activations=" << generators.activations << " spawns=" << generators.spawns << '\n';
		std::cout << "Level two route result: key=" << key_id << " frames=" << frames << " dead=" << Player_is_dead
		          << " shields=" << Players[Player_num].shields << " complete=" << complete() << '\n';
		require(!Player_is_dead && Players[Player_num].lives == entry_lives && complete(), "level-two ordinary key route incomplete");
		const auto energy = Players[Player_num].energy, shields = Players[Player_num].shields;
		char blue_save[] = "xl-level-two-blue.sav", red_save[] = "xl-level-two-red.sav", description[] = "Earned level two key";
		char *save = second_level == 1 ? blue_save : red_save;
		const auto generator_state = generator_checkpoint_state();
		stop_time();
		require(state_save_all_sub(save, description) && state_restore_all_sub(save, 0), "level-two earned key checkpoint failed");
		require(generator_checkpoint_state() == generator_state, "earned checkpoint changed generator masks, activation counters or timing");
		std::cout << "Level two generator checkpoint: masks, stations, capacity, lives and timers restored\n";
		require(Current_level_num == 2 && (Players[Player_num].flags & key_flag) &&
		            Players[Player_num].energy == energy && Players[Player_num].shields == shields,
		        "level-two earned checkpoint altered inventory");
		std::cout << "Level two ordinary " << (second_level == 1 ? "blue" : "red") << " key: frames=" << frames << " alive=1 checkpoint=restored\n";
		return;
	}
	if (campaign) {
		require(!Player_is_dead && Players[Player_num].lives == entry_lives && transition_countdown > 0 &&
		            Current_level_num == 2 && !std::strcmp(Current_mission_filename, "d2-xl") &&
		            !std::strcmp(Gamesave_current_filename, "d2leva-2.rl2") && xl_preview_pilot_active(),
		        "earned exit did not reach original second level alive before countdown");
		const auto position = ConsoleObject->pos;
		const auto energy = Players[Player_num].energy, shields = Players[Player_num].shields;
		char save[] = "xl-earned-level-two.sav", description[] = "Earned level two arrival";
		stop_time();
		require(state_save_all_sub(save, description), "earned level-two checkpoint failed");
		require(state_restore_all_sub(save, 0), "earned level-two checkpoint restore failed");
		require(Current_level_num == 2 && xl_preview_pilot_active() && vm_vec_dist(&position, &ConsoleObject->pos) == 0 &&
		            Players[Player_num].energy == energy && Players[Player_num].shields == shields && Players[Player_num].lives == entry_lives,
		        "earned level-two checkpoint changed arrival state");
		std::cout << "Earned campaign transition: level=2 alive=1 frames=" << frames << " prior_countdown=" << transition_countdown
		          << "; original identity and arrival checkpoint restored\n";
		return;
	}
	if (visit >= 0) {
		if (cross_from >= 0) std::cout << "Normal linked portal: from=" << cross_from << " to=" << visit << " crossed=" << crossed << '\n';
		std::cout << "Normal secret entry: target=" << visit << " actual=" << ConsoleObject->segnum << " frames=" << frames
		          << " dead=" << Player_is_dead << " shields=" << Players[Player_num].shields
		          << " reactor=" << Control_center_destroyed << " countdown=" << Countdown_timer << '\n';
		require(!Player_is_dead && frames > 0 && complete() && find_point_seg(&ConsoleObject->pos, visit) == visit,
		        "normal-input secret entry incomplete");
		return;
	}
	std::cout << "Normal route trigger coverage:";
	for (int t = 0; t < Num_triggers; ++t)
		std::cout << " " << t << "=" << bool(route_activated_triggers & (uint64_t(1) << t))
		          << "/" << bool(route_passable_targets & (uint64_t(1) << t));
	std::cout << " (activated/linked-targets-passable; exit has no links)\n";
	if (branch >= 0) {
		std::cout << "Normal branch navigation: trigger=" << branch << " frames=" << frames << " dead=" << Player_is_dead
		          << " activated=" << bool(xl_preview_trigger_mask() & (uint64_t(1) << branch))
		          << " targets_passable=" << bool(route_passable_targets & (uint64_t(1) << branch)) << '\n';
		require(!Player_is_dead && complete(), "normal-input authored branch incomplete");
		verify_branch_checkpoint(branch);
		return;
	}
	if (escape) {
		input_demo_stop_quick_recording_common();
		const auto recordings = std::filesystem::path(PHYSFS_getWriteDir()) / "input_demo_recordings" / "new";
		bool recorded = false;
		if (std::filesystem::is_directory(recordings))
			for (const auto &entry : std::filesystem::directory_iterator(recordings))
				if (entry.path().extension() == ".dximdemo") {
					std::ifstream file(entry.path());
					std::string line;
					std::getline(file, line);
					const auto header = nlohmann::json::parse(line);
					require(header.at("mission") == "d2-xl" && header.at("start_mode") == "save_checkpoint" && header.at("frame_count") == frames,
					        "escape recording lacks the earned checkpoint or complete frame count");
					std::filesystem::copy_file(entry.path(), recordings.parent_path().parent_path() / "xl-normal-escape.dximdemo", std::filesystem::copy_options::overwrite_existing);
					recorded = true;
				}
		require(recorded, "earned escape input recording was not written");
		require(xl_checkpoint_finish_export("xl-normal-exit-approach.sav") != 0, "cannot append native source definitions to exit checkpoint");
		require(write_player_file() == 0, "cannot export the recorded native player profile");
		std::cout << "Normal escape recording: earned checkpoint and ordered flight inputs exported; replay and interactive completion unverified\n";
		std::cout << "Normal escape navigation: frames=" << frames << " dead=" << Player_is_dead
		          << " completed=" << xl_preview_pilot_completed() << " level=" << Current_level_num
		          << " countdown=" << Countdown_timer << " shields=" << Players[Player_num].shields << '\n';
		require(!Player_is_dead && xl_preview_pilot_completed() && Current_level_num == 1 && Countdown_timer > 0,
		        "normal-input timed exit incomplete");
		return;
	}
	if (reactor) {
		std::cout << "Normal reactor navigation: frames=" << frames << " dead=" << Player_is_dead
		          << " destroyed=" << Control_center_destroyed << " shields=" << Players[Player_num].shields
		          << " countdown=" << Countdown_timer << "; exit unverified\n";
		require(!Player_is_dead && Control_center_destroyed && Countdown_timer > 0, "normal-input reactor objective incomplete");
		char checkpoint[] = "xl-normal-reactor.sav", description[] = "Normal input reactor";
		// The native low-level save consumes its caller's pause
		stop_time();
		require(state_save_all_sub(checkpoint, description) != 0, "normal reactor checkpoint failed");
		return;
	}
	std::cout << "Normal red-key navigation: frames=" << frames << " dead=" << Player_is_dead
	          << " red_key=" << bool(Players[Player_num].flags & KEY_RED) << " kills=" << Players[Player_num].num_kills_level
	          << "; seed=1, reactor/exit unverified\n";
	require(!Player_is_dead && (Players[Player_num].flags & KEY_RED), "normal-input red-key navigation incomplete");
	char checkpoint[] = "xl-normal-key.sav", description[] = "Normal input red key";
	// The native low-level save consumes its caller's pause
	stop_time();
	require(state_save_all_sub(checkpoint, description) != 0, "normal red-key checkpoint failed");
}
void mission_isolation(const xl_compat::Inventory &source, int vertex)
{
	char error[256] = {};
	auto start_level = [] {
		// This flag is consumed by each startup, not a persistent test setting
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
	};
	// Capture fresh stock definitions, then return through a modified pilot session
	std::fputs("XL isolation: selecting stock mission\n", stderr);
	require(load_base_mission("d2", error, sizeof(error)), error);
	std::fputs("XL isolation: starting stock level\n", stderr);
	start_level();
	std::fputs("XL isolation: stock level started\n", stderr);
	require(!std::strcmp(Current_mission_filename, "d2"), "stock mission selection failed");
	const auto stock_archive = PHYSFS_getRealDir(Gamesave_current_filename);
	require(stock_archive && !d_stricmp(std::filesystem::path(stock_archive).filename().string().c_str(), "descent2.hog"), "stock level did not resolve from retail archive");
	unchanged(vertex);
	const auto stock_definitions = gameplay_definitions();
	const auto stock_lights = face_lights();
	require(load_base_mission("d2-xl", error, sizeof(error)), error);
	start_level();
	depth_policy(true);
	const auto pilot_definitions = gameplay_definitions();
	const auto pilot_lights = face_lights();
	colored(source, vertex);
	materials(source);
	const auto &light = source.light_indices.front();
	require(subtract_light(light[0], light[1]) == 1, "isolation fixture light did not switch off");
	Walls[6].cloak_value = 11;
	require(load_base_mission("d2", error, sizeof(error)), error);
	start_level();
	unchanged(vertex);
	require(gameplay_definitions() == stock_definitions, "pilot leaked gameplay definitions into stock mission");
	require(face_lights() == stock_lights, "pilot leaked light state into stock mission");
	require(load_base_mission("d2-xl", error, sizeof(error)), error);
	start_level();
	require(!std::strcmp(Gamesave_current_filename, "d2leva-1.rl2"), "pilot return lost original identity");
	require(gameplay_definitions() == pilot_definitions, "pilot definitions changed across stock transition");
	require(face_lights() == pilot_lights, "pilot return retained switched-off light state");
	colored(source, vertex);
	materials(source);
	object_dependencies();
	std::cout << "Mission isolation: pilot -> stock -> pilot restores fresh lighting/materials and robot/weapon/powerup/joint definitions; distinct custom override rollback unverified\n";
}
} // namespace

#include "arena_test.h"
#include "pog_test.h"
#include "campaign_test.h"
#include "asset_mount_test.h"
#include "static_texture_test.h"
#include "format17_test.h"
#include "format23_test.h"
#include "descriptor_test.h"
#include "package_audit.h"
#include "konflict_test.h"
#include "neptune_test.h"
#include "modern_properties_test.h"
#include "teleport_test.h"
#include "trigger_test.h"
#include "scheduler_test.h"
#include "object_trigger_test.h"
#include "generator_test.h"
#include "boss_spawn_test.h"
#include "equipment_test.h"
#include "effect_inspection_test.h"
#include "boss_transport_test.h"
#include "trigger_flags_test.h"
#include "boss_groups_test.h"
#include "player_resources_test.h"
#include "object_chain_delay_test.h"
#include "speed_boost_test.h"
#include "spawn_points_test.h"
#include "spawn_points_lifecycle_test.h"
#include "water_actors_test.h"
#include "lava_test.h"
#include "blocked_test.h"
#include "keyed_wall_test.h"
#include "doorway_test.h"
#include "close_door_test.h"
#include "wide_wall_test.h"
#include "cloak_test.h"
#include "trigger_capacity_test.h"
#include "projectile_test.h"
#include "object_capacity_test.h"
#include "changing_light_test.h"
extern "C" void flicker_lights(void);
#include "flicker_test.h"
#include "hog_test.h"
#include "repair_test.h"
#include "format34_test.h"
#include "runtime_layout_test.h"
#include "level_message_test.h"
#include "texture_action_test.h"
#include "countdown_test.h"
#include "disarm_test.h"
#include "named_audio_test.h"
#include "classic_audio_test.h"
#include "audio_scope_test.h"
#include "hxm_test.h"

int main(int argc, char **argv)
{
	xl_install_crash_diagnostic();
	if (find_arg_value(argc, argv, "-texture-action-test")) return texture_action_test(argc, argv);
	if (find_arg_value(argc, argv, "-countdown-test")) return countdown_test(argc, argv);
	if (find_arg_value(argc, argv, "-disarm-test")) return disarm_test(argc, argv);
	if (find_arg_value(argc, argv, "-named-audio-original")) return named_audio_original(argc, argv);
	if (find_arg_value(argc, argv, "-classic-audio-original")) return classic_audio_original(argc, argv);
	if (find_arg_value(argc, argv, "-named-audio-test")) return named_audio_test(argc, argv);
	if (find_arg_value(argc, argv, "-audio-scope-test")) return audio_scope_test(argc, argv);
	if (find_arg_value(argc, argv, "-pog-test")) return pog_test(argc, argv);
	if (find_arg_value(argc, argv, "-level-message-test")) return level_message_test(argc, argv);
	if (find_arg_value(argc, argv, "-runtime-layouts-test")) return runtime_layout_test(argc, argv);
	if (find_arg_value(argc, argv, "-hog-test")) return hog_test(argc, argv);
	if (find_arg_value(argc, argv, "-flicker-test")) return flicker_test(argc, argv);
	if (find_arg_value(argc, argv, "-changing-light-test")) return changing_light_test(argc, argv);
	if (find_arg_value(argc, argv, "-object-capacity-test")) return object_capacity_test(argc, argv);
	if (find_arg_value(argc, argv, "-projectile-test")) return projectile_test(argc, argv);
	if (find_arg_value(argc, argv, "-format34-test")) return format34_test(argc, argv);
	if (find_arg_value(argc, argv, "-hxm-test")) return hxm_test(argc, argv);
	if (find_arg_value(argc, argv, "-blocked-test")) return blocked_test(argc, argv);
	if (find_arg_value(argc, argv, "-close_door-test")) return close_door_test(argc, argv);
	if (find_arg_value(argc, argv, "-widewall-test")) return wide_wall_test(argc, argv);
	if (find_arg_value(argc, argv, "-trigger-capacity-test")) return trigger_capacity_test(argc, argv);
	if (find_arg_value(argc, argv, "-cloak-test")) return cloak_test(argc, argv);
	if (find_arg_value(argc, argv, "-doorway-test")) return doorway_test(argc, argv);
	if (find_arg_value(argc, argv, "-keyed_wall-test")) return keyed_wall_test(argc, argv);
	if (find_arg_value(argc, argv, "-repair-test")) return repair_test(argc, argv);
	if (find_arg_value(argc, argv, "-lava-test")) return lava_test(argc, argv);
	if (find_arg_value(argc, argv, "-water-actors-test")) return water_actors_test(argc, argv);
	if (find_arg_value(argc, argv, "-speed-boost-test")) return speed_boost_test(argc, argv);
	if (find_arg_value(argc, argv, "-spawn-points-test")) return spawn_points_test(argc, argv);
	if (find_arg_value(argc, argv, "-object-chain-delay-test")) return object_chain_delay_test(argc, argv);
	if (find_arg_value(argc, argv, "-boss-transport-test")) return boss_transport_test(argc, argv);
	if (find_arg_value(argc, argv, "-trigger-flags-test")) return trigger_flags_test(argc, argv);
	if (find_arg_value(argc, argv, "-boss-groups-test")) return boss_groups_test(argc, argv);
	if (find_arg_value(argc, argv, "-player-resources-test")) return player_resources_test(argc, argv);
	if (find_arg_value(argc, argv, "-effect-inspection-test")) return effect_inspection_test(argc, argv);
	if (find_arg_value(argc, argv, "-equipment-test")) return equipment_test(argc, argv);
	if (find_arg_value(argc, argv, "-boss-spawn-test")) return boss_spawn_test(argc, argv);
	if (find_arg_value(argc, argv, "-generator-assignment-test")) return generator_assignment_test(argc, argv);
	if (find_arg_value(argc, argv, "-object-trigger-test")) return object_trigger_test(argc, argv);
	if (find_arg_value(argc, argv, "-trigger-scheduler-test")) return trigger_scheduler_test(argc, argv);
	if (find_arg_value(argc, argv, "-trigger-chain-test")) return trigger_chain_test(argc, argv);
	if (find_arg_value(argc, argv, "-modern-properties-test")) return modern_properties_test(argc, argv);
	if (find_arg_value(argc, argv, "-teleport-test")) return teleport_test(argc, argv);
	if (find_arg_value(argc, argv, "-neptune-test")) return neptune_test(argc, argv);
	if (find_arg_value(argc, argv, "-format23-test")) return format23_layout_test(argc, argv);
	if (find_arg_value(argc, argv, "-konflict-test")) return konflict_test(argc, argv);
	if (find_arg_value(argc, argv, "-static-texture-test")) return static_texture_catalog_test(argc, argv);
	if (find_arg_value(argc, argv, "-package-audit")) return package_audit(argc, argv);
	if (find_arg_value(argc, argv, "-descriptor-test")) return descriptor_test(argc, argv);
	if (find_arg_value(argc, argv, "-arena-test")) return arena_test(argc, argv);
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		const auto source_path = find_arg_value(argc, argv, "-xl-source");
		const auto fixture_path = find_arg_value(argc, argv, "-fixture-path");
		require(source_path && fixture_path, "requires original source and private fixture path");
		const auto extra_dir = find_arg_value(argc, argv, "-extra-dir");
		require(extra_dir != nullptr, "requires private fixture directory");
		require(std::filesystem::weakly_canonical(fixture_path) == std::filesystem::weakly_canonical(std::filesystem::path(extra_dir) / "xlprobe.rl2"), "fixture path must identify the mounted test fixture");
		require(!std::filesystem::equivalent(source_path, fixture_path), "original source cannot be the test fixture");
		std::ifstream stream(source_path, std::ios::binary);
		require(bool(stream), "cannot read original source");
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		const auto probe = xl_compat::prepare_source_runtime_probe(bytes);
		std::ifstream fixture_stream(fixture_path, std::ios::binary);
		const xl_compat::Bytes fixture_bytes{ std::istreambuf_iterator<char>(fixture_stream), std::istreambuf_iterator<char>() };
		require(fixture_bytes == probe.stock_level, "private fixture must initially match source translation");
		fixture_stream.close();
		int vertex = -1;
		for (unsigned i = 0; i < probe.source.vertex_colors.size(); ++i) {
			const auto &c = probe.source.vertex_colors[i].rgb;
			if (c[0] != c[1] || c[1] != c[2]) {
				vertex = int(i);
				break;
			}
		}
		require(vertex >= 0, "fixture has no chromatic vertex");
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission(find_arg_value(argc, argv, "-mission"), error, sizeof(error)), error);
		unchanged(vertex);
		require(load_level("d2leva-1.rl2") == 0, "original XL pilot admission failed");
		{
			const auto &source = probe.source;
			const auto section = std::find_if(source.sections.begin(), source.sections.end(), [](const auto &s) { return s.name == "light_indices"; });
			const auto index = std::find_if(source.light_indices.begin(), source.light_indices.end(), [&](const auto &light) {
				return source.delta_lights.size() - light[3] >= 255;
			});
			require(section != source.sections.end() && index != source.light_indices.end(), "large-count lighting fixture unavailable");
			const auto record = std::size_t(index - source.light_indices.begin());
			const auto path = (std::filesystem::path(extra_dir) / "widelight.rl2").string();
			for (const int count : { 128, 255 }) {
				auto wide = bytes;
				wide.at(section->offset + record * 6 + 3) = static_cast<std::uint8_t>(count);
				const auto inventory = xl_compat::inspect_level(wide);
				write_bytes(path.c_str(), wide);
				require(xl_level_supported("widelight.rl2", 0) && load_level("widelight.rl2") == 0, "large light count rejected by normal admission");
				require_owned_light_records(inventory);
				changing_lights(inventory, "widelight.rl2");
				std::cout << "XL split light count " << count << ": normal admission, native records and all source light-off/on UVLs verified\n";
			}
			require(load_level("d2leva-1.rl2") == 0, "original pilot recovery after split lights failed");
		}
		{
			const int saved_robot_count = N_robot_types;
			N_robot_types = 0;
			const int accepted = xl_level_supported("d2leva-1.rl2", 0);
			N_robot_types = saved_robot_count;
			require(!accepted && std::strstr(xl_level_rejection(), "robot"), "missing robot dependency was not explained");
			require(xl_preview_pilot_active(), "preflight rejection changed active XL state");
			require(xl_level_supported("d2leva-1.rl2", 0), "dependency preflight did not recover");
		}
		{
			auto renamed = bytes;
			const auto palette_end = std::find(renamed.begin() + 23, renamed.end(), '\n');
			require(palette_end != renamed.end(), "missing palette terminator");
			renamed.at(std::size_t(palette_end - renamed.begin()) + 1) ^= 1;
			const auto renamed_path = (std::filesystem::path(extra_dir) / "renamed.rl2").string();
			write_bytes(renamed_path.c_str(), renamed);
			char other_mission[] = "different";
			char *saved_name = Current_mission->filename;
			const int saved_level = Current_level_num;
			Current_mission->filename = other_mission;
			Current_level_num = 2;
			const bool eligible = xl_level_supported("renamed.rl2", 0) != 0;
			const int loaded = load_level("renamed.rl2");
			const bool active = xl_preview_pilot_active() != 0;
			Current_mission->filename = saved_name;
			Current_level_num = saved_level;
			require(eligible && !loaded && active, "feature-compatible renamed/rehashed level rejected");
			require(!std::strcmp(Gamesave_current_filename, "renamed.rl2"), "renamed source identity lost");
			std::cout << "XL detection admission: different mission, level name, level number and source hash accepted\n";
			require(load_level("d2leva-1.rl2") == 0, "original level recovery failed");
		}
		require(!std::strcmp(Gamesave_current_filename, "d2leva-1.rl2"), "translation changed level identity");
		colored(probe.source, vertex);
		materials(probe.source);
		material_passage(probe.source);
		material_targeting(probe.source);
		texture_emitters(probe.source);
		// Controlled light subtraction can clamp UVLs; reload before later assertions
		require(load_level("d2leva-1.rl2") == 0, "pilot reload after emitter probes failed");
		const int previous_mode = Game_mode;
		for (const int mode : { GM_NETWORK, GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS }) {
			Game_mode = mode;
			require(load_level("d2leva-1.rl2") != 0, "original pilot accepted multiplayer loading");
			unchanged(vertex);
			require(load_level("xlprobe.rl2") != 0, "translated preview bypassed multiplayer rejection");
			unchanged(vertex);
			require(load_level("stockprobe.rl2") == 0, "XL admission guard blocked a stock-format multiplayer level");
			unchanged(vertex);
		}
		Game_mode = previous_mode;
		require(load_level("d2leva-1.rl2") == 0, "single-player pilot did not recover after multiplayer rejection");
		colored(probe.source, vertex);
		materials(probe.source);
		std::cout << "XL multiplayer admission: original and translated preview rejected in network/cooperative modes, stock loader and single-player recovery passed\n";
		const auto original_copy = (std::filesystem::path(extra_dir) / "d2leva-1.rl2").string();
		const auto native_state = [] {
			return std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments) +
			       std::string(reinterpret_cast<const char *>(Vertices), sizeof(*Vertices) * Num_vertices) +
			       std::string(reinterpret_cast<const char *>(Objects), sizeof(*Objects) * (Highest_object_index + 1)) +
			       std::string(reinterpret_cast<const char *>(Light_subtracted), sizeof(*Light_subtracted) * Num_segments) +
			       Gamesave_current_filename;
		};
		const auto before_rejection = native_state();
		auto altered_original = bytes;
		altered_original[4] = 16;
		write_bytes(original_copy.c_str(), altered_original);
		const int original_rejected = load_level("d2leva-1.rl2");
		write_bytes(original_copy.c_str(), bytes);
		require(original_rejected != 0, "unsupported level version accepted");
		require(native_state() == before_rejection, "unsupported candidate changed native geometry, objects, light masks or identity");
		require(std::strstr(xl_level_rejection(), "version"), "unsupported version rejection lacks a reason");
		require(xl_preview_pilot_active(), "unsupported candidate cleared the active pilot");
		colored(probe.source, vertex);
		materials(probe.source);
		require(load_level("d2leva-1.rl2") == 0, "original pilot recovery failed");
		std::cout << "Pilot ambient flags:";
		for (int segment = 0; segment < Num_segments; ++segment)
			std::cout << ' ' << int(Segment2s[segment].s2_flags);
		std::cout << '\n';
		std::vector<int> loaded_ambient;
		for (int segment = 0; segment < Num_segments; ++segment) loaded_ambient.push_back(Segment2s[segment].s2_flags);
		set_ambient_sound_flags();
		std::cout << "Pilot ambient flags after active-material recompute:";
		for (int segment = 0; segment < Num_segments; ++segment) {
			std::cout << ' ' << int(Segment2s[segment].s2_flags);
			require(Segment2s[segment].s2_flags == loaded_ambient[segment], "active material recompute changed ambient regions");
			Segment2s[segment].s2_flags = loaded_ambient[segment];
		}
		std::cout << '\n';
		for (int segment = 80; segment <= 87; ++segment)
			for (int side = 0; side < 6; ++side) {
				const auto &s = Segments[segment].sides[side];
				std::cout << "Ambient side: segment=" << segment << " side=" << side << " texture=" << s.tmap_num << " overlay=" << (s.tmap_num2 & 0x3fff)
				          << " texture_flags=" << int(TmapInfo[s.tmap_num].flags) << " overlay_flags=" << int(TmapInfo[s.tmap_num2 & 0x3fff].flags)
				          << " doorway=" << WALL_IS_DOORWAY(&Segments[segment], side) << '\n';
			}
		// ar12 computes ambience before texture 269 is paged in. Its then-opaque
		// boundary is a diagnostic control, not a material rule to copy into Redux
		require(!(TmapInfo[175].flags & (TMI_VOLATILE | TMI_WATER)), "ambient control texture emits sound");
		std::vector<side *> ambient_control_sides;
		for (int segment = 0; segment < Num_segments; ++segment)
			for (auto &side : Segments[segment].sides)
				if (side.tmap_num == 269) {
					ambient_control_sides.push_back(&side);
					side.tmap_num = 175;
				}
		require(!(WALL_IS_DOORWAY(&Segments[83], 4) & WID_RENDPAST_FLAG), "ambient negative control remains transparent");
		set_ambient_sound_flags();
		int ambient_differences = 0;
		for (int segment = 0; segment < Num_segments; ++segment) {
			const int expected = segment == 81 || segment == 82 ? loaded_ambient[segment] & ~S2F_AMBIENT_LAVA : loaded_ambient[segment];
			require(Segment2s[segment].s2_flags == expected, "opaque boundary changed unexpected ambient region");
			const int reference = probe.source.segments[segment].flags | ((segment == 51 || segment == 111) ? S2F_AMBIENT_LAVA : 0);
			require(Segment2s[segment].s2_flags == reference, "opaque boundary does not reproduce pinned ar12 ambient flags");
			ambient_differences += Segment2s[segment].s2_flags != loaded_ambient[segment];
			Segment2s[segment].s2_flags = loaded_ambient[segment];
		}
		for (auto side : ambient_control_sides) side->tmap_num = 269;
		require(ambient_differences == 2, "ambient boundary control did not isolate both regions");
		std::cout << "Ambient boundary control: only segments 81/82 lose lava flags with texture 269 opaque; original textures and flags restored\n";
		if (const auto hog = find_arg_value(argc, argv, "-pilot-hog"); hog && *hog) {
			require(PHYSFS_addToSearchPath(hog, 0) != 0, "cannot mount original pilot HOG");
			const auto resolved = PHYSFS_getRealDir("d2leva-1.rl2");
			require(resolved && std::filesystem::equivalent(hog, resolved), "pilot did not resolve from original archive");
			require(load_level("d2leva-1.rl2") == 0, "archived original pilot rejected");
			colored(probe.source, vertex);
			materials(probe.source);
			require(PHYSFS_removeFromSearchPath(hog) != 0, "original pilot archive remained mounted");
			std::cout << "Original HOG pilot: resolved, admitted, colors/materials retained, unmounted\n";
		}
		require(load_level("xlprobe.rl2") == 0, "valid preview load rejected");
		colored(probe.source, vertex);
		materials(probe.source);
		flash_lighting(vertex);
		dynamic_lighting(probe.source, vertex);
		changing_lights(probe.source);
		require(load_level("stockprobe.rl2") == 0, "stock alias load rejected");
		unchanged(vertex);
		require(load_level("xlprobe.rl2") == 0, "preview reload rejected");
		colored(probe.source, vertex);
		// Change a coordinate bit without changing counts or file size
		auto corrupt = probe.stock_level;
		corrupt.at(probe.source.mine_offset + 5) ^= 1;
		write_bytes(fixture_path, corrupt);
		const auto before_mismatch = native_state();
		const int rejected = load_level("xlprobe.rl2");
		write_bytes(fixture_path, probe.stock_level);
		require(rejected != 0, "mismatched source/fixture accepted");
		require(native_state() == before_mismatch, "mismatched fixture was loaded before validation");
		colored(probe.source, vertex);
		materials(probe.source);
		std::cout << "XL rejected candidates preserve native geometry, objects, light masks, identity and active shading/materials before commit\n";
		require(load_level("xlprobe.rl2") == 0, "restored preview load rejected");
		colored(probe.source, vertex);
		std::cout << "XL preview lifecycle: activate, clear, reload, reject mismatch, recover passed; rendering/gameplay unverified\n";
		for (int i = 1; i < argc; ++i)
			if (!std::strcmp(argv[i], "-xl-test-save")) {
				// Retail and XL use the same member name; remove loose-fixture shadowing
				std::filesystem::rename(original_copy, std::filesystem::path(extra_dir) / "pilot-source-copy.rl2");
				original_mission_save(probe.source, vertex);
				normal_input_frames();
				mission_isolation(probe.source, vertex);
				if (std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-navigation"); })) navigate_objective(false);
				if (const char *branch = find_arg_value(argc, argv, "-xl-test-branch")) navigate_objective(true, false, std::stoi(branch));
				if (std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-reactor"); })) navigate_objective(true);
				if (const char *visit = find_arg_value(argc, argv, "-xl-test-visit")) navigate_objective(true, false, -1, std::stoi(visit));
				if (const char *cross = find_arg_value(argc, argv, "-xl-test-cross")) {
					const std::string edge(cross);
					const auto separator = edge.find(':');
					require(separator != std::string::npos, "crossing requires from:to");
					navigate_objective(true, false, -1, std::stoi(edge.substr(separator + 1)), std::stoi(edge.substr(0, separator)));
				}
				if (std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-escape"); })) {
					const bool campaign = std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-campaign"); });
					navigate_objective(true, true, -1, -1, -1, campaign);
					if (std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-level-two-route"); })) {
						navigate_objective(false, false, -1, -1, -1, false, 1);
						navigate_objective(false, false, -1, -1, -1, false, 2);
					}
					mission_isolation(probe.source, vertex);
				}
				if (!std::any_of(argv, argv + argc, [](const char *arg) { return !std::strcmp(arg, "-xl-test-navigation"); })) campaign_level_two();
			}
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL lifecycle FAILED: " << error.what() << '\n';
		return 1;
	}
}
