// Detected XL requirements and runtime integration; developer previews remain explicit
#include "preview.h"
#include "projectile.h"
#include "speed_boost.h"
#include "spawn_points.h"
#include "fluids.h"
#include "blocked.h"
#include "repair.h"
#include "doorway.h"
#include "object_trigger.h"
#include "robot_generator.h"
#include "boss_spawn.h"
#include "equipment.h"
#include "no_damage_test_binding.h"
#include "resource_validation.h"
#include "light_sources.h"
#include "companion_resources.h"
#include "../probe.h"
#include "../lighting.h"
#include "../admission.h"
#include "texture_brightness.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cctype>
#include "asset_dependencies.h"
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "cntrlcen.h"
#include "object.h"
#include "robot.h"
#include "render.h"
#include "lighting.h"
#include "player.h"
#include "weapon.h"
#include "laser.h"
#include "vclip.h"
#include "rle.h"
#include "rng_shared.h"
#include "args.h"
#include "console.h"
#include "mission.h"
#include "physfsx.h"
#include "strutil.h"
#include "gameseq.h"
#include "game.h"
#include "switch.h"
#include "textures.h"
#include "gameseg.h"
#include "fvi.h"
#include "dxxerror.h"
#include "digi.h"
#include "sounds.h"
#include "bm.h"
#include <physfs.h>
int xl_stock_load_level(const char *filename);
int xl_stock_wall_is_doorway(segment *seg, int side);
extern fix flash_scale, Seismic_tremor_magnitude;
extern int Do_dynamic_light;
}

namespace
{
std::unique_ptr<xl_compat::Inventory> source;
std::unique_ptr<xl_compat::ClassicAmbientLighting> classic_ambient;
std::vector<xl_compat::Rgb> classic_colors;
std::vector<xl_compat::Rgb> rebuilt_ambient;
#ifdef DXX_XL_DESCRIPTOR_TEST
std::unique_ptr<xl_compat::Inventory> no_damage_test_source;
#endif
bool water_test_profile = false;
bool classic_light_test_profile = false;
uint64_t vertex_calls = 0;
uint64_t corona_draws = 0;
uint64_t water_draws = 0, water_depth_failures = 0;
uint64_t corona_depth_failures = 0;
uint64_t corona_classic_requested_draws = 0;
float face_alpha = -1;
bool face_water_material = false;
std::vector<xl_compat::DynamicVertexLight> dynamic_vertices;
std::vector<float> brightness;
unsigned unresolved_emitters = 0;
struct MuzzleLight {
	vms_vector position;
	fix intensity;
};
std::vector<MuzzleLight> muzzle_lights;
vms_vector player_thrust{};
std::string pending_level;
bool loading_wide_walls = false;
bool loading_extended_triggers = false;
bool admitted_pilot = false, completed_pilot = false, admitted_water_arena = false;
std::string rejection;
std::array<bool, DXX_XL_MAX_TRIGGERS> activated_triggers{};
constexpr const char *translated_path = "xl_preview_cache/pilot.rl2";
// XL palette.h uses 31; stock GR_FADE_LEVELS is 34 and cannot be reused
constexpr int xl_fade_levels = 31;
int active_cloak_wall = -1, drawn_cloak_wall = -1, drawn_cloak_value = 0;
uint64_t cloak_draws = 0;
float drawn_cloak_alpha = 0;
bool missile_light(unsigned id)
{
	// Pinned XL CObject::InitTables and include/laser.h, including robot IDs
	// Mines and smart children are deliberately not missiles
	constexpr unsigned ids[] = { 8, 15, 17, 18, 21, 22, 28, 36, 37, 39, 40, 50, 54, 55, 57, 58, 59, 61, 62 };
	return std::find(std::begin(ids), std::end(ids), id) != std::end(ids);
}
xl_compat::Rgb animation_frame_color(bitmap_index index)
{
	if (index.index >= Num_bitmap_files) throw std::runtime_error("XL animation bitmap ID invalid");
	PIGGY_PAGE_IN(index);
	const auto &bitmap = GameBitmaps[index.index];
	if (!bitmap.bm_data || bitmap.bm_type != BM_LINEAR || bitmap.bm_w <= 0 || bitmap.bm_h <= 0)
		throw std::runtime_error("XL animation bitmap unavailable");
	unsigned average = bitmap.avg_color;
	if (!average) {
		// XL AvgColorIndex ignores black and both transparency indices
		std::int64_t sum[3] = {}, count = 0;
		std::vector<ubyte> row(bitmap.bm_w);
		const bool compressed = (bitmap.bm_flags & BM_FLAG_RLE) != 0;
		const int row_bytes = bitmap.bm_flags & BM_FLAG_RLE_BIG ? 2 : 1;
		const ubyte *encoded = compressed ? bitmap.bm_data + 4 + bitmap.bm_h * row_bytes : nullptr;
		for (int y = 0; y < bitmap.bm_h; ++y) {
			const ubyte *pixels = bitmap.bm_data + y * bitmap.bm_rowsize;
			if (compressed) {
				gr_rle_expand_scanline(row.data(), const_cast<ubyte *>(encoded), 0, bitmap.bm_w - 1);
				const auto *size = bitmap.bm_data + 4 + y * row_bytes;
				encoded += size[0] + (row_bytes == 2 ? unsigned(size[1]) * 256 : 0);
				pixels = row.data();
			}
			for (int x = 0; x < bitmap.bm_w; ++x) {
				const unsigned c = pixels[x];
				if (!c || c == 254 || c == 255) continue;
				for (unsigned channel = 0; channel < 3; ++channel) sum[channel] += gr_palette[c * 3 + channel];
				++count;
			}
		}
		if (count) {
			int best = (std::numeric_limits<int>::max)();
			// XL ClosestColor includes index 254; stock excludes it
			for (unsigned i = 0; i < 255; ++i) {
				int distance = 0;
				for (unsigned c = 0; c < 3; ++c) {
					const int delta = int(sum[c] / count) - gr_palette[i * 3 + c];
					distance += delta * delta;
				}
				if (distance < best) {
					best = distance;
					average = i;
				}
			}
		}
	}
	return { gr_palette[average * 3] / 255.0f, gr_palette[average * 3 + 1] / 255.0f, gr_palette[average * 3 + 2] / 255.0f };
}
const char *option(const char *name)
{
	for (int i = 1; i < Num_args; ++i)
		if (!std::strcmp(Args[i], name)) return i + 1 < Num_args ? Args[i + 1] : "";
	return nullptr;
}

xl_compat::Bytes read_extended_level(const char *filename)
{
	std::string path = filename;
	if (!PHYSFSX_exists(path.c_str(), 1)) path = std::string(MISSION_DIR) + filename;
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
	if (!input) throw std::runtime_error("level file is missing: " + std::string(filename));
	unsigned char header[8];
	if (PHYSFS_read(input.get(), header, 1, 8) != 8) throw std::runtime_error("level header is truncated");
	const uint32_t version = uint32_t(header[4]) | uint32_t(header[5]) << 8 | uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
	if (std::memcmp(header, "LVLP", 4) || version <= 8) return {};
	const auto size = PHYSFS_fileLength(input.get());
	if (size < 8 || size > 64 * 1024 * 1024) throw std::runtime_error("XL level exceeds the supported 64 MiB limit");
	xl_compat::Bytes bytes(static_cast<std::size_t>(size));
	PHYSFS_seek(input.get(), 0);
	if (PHYSFS_read(input.get(), bytes.data(), 1, PHYSFS_uint32(bytes.size())) != size) throw std::runtime_error("XL level read failed");
	return bytes;
}

// Experimental fluid profile selected by detected requirements, never mission identity
bool water_profile(const xl_compat::Bytes &bytes, bool multiplayer)
{
	const auto level = xl_compat::inspect_level(bytes);
	const bool water = std::any_of(level.segments.begin(), level.segments.end(), [&](const xl_compat::Segment &segment) {
		return (xl_compat::segment_property_bits(level.level_version, segment) & xl_compat::segment_water) != 0;
	});
	if (!water) return false;
	if (level.level_version < 21 && !option("-xl-water-physics") && !water_test_profile)
		throw std::runtime_error("water physics: the experimental fluid profile is not enabled; Android and presentation qualification is incomplete");
	if (multiplayer) throw std::runtime_error("water physics multiplayer synchronization is unqualified");
	if (level.level_version < 21 && (!level.matcens.empty() || std::any_of(level.objects.begin(), level.objects.end(), [](const xl_compat::Object &object) { return object.type == OBJ_ROBOT || (object.contains_count > 0 && object.contains_type == OBJ_ROBOT); })))
		throw std::runtime_error("water with robots: AI combat and robot generation in fluids remain unqualified");
	return true;
}

void validate_mission_context()
{
	if (const char *reason = xl_resource_base_rejection(); *reason)
		throw std::runtime_error(reason);
}

bool mission_has_companion(const char *extension)
{
	auto exists = [&](const char *level) {
		std::string name = level;
		const auto dot = name.find_last_of('.');
		if (dot != std::string::npos) name.resize(dot);
		name += extension;
		// Use the same mounted lookup as the native level companion loaders
		return PHYSFSX_exists(name.c_str(), 1) != 0;
	};
	for (int i = 0; i < Last_level; ++i)
		if (exists(Level_names[i])) return true;
	for (int i = 0; i < -Last_secret_level; ++i)
		if (exists(Secret_level_names[i])) return true;
	return false;
}

void validate_requirements(const xl_compat::Inventory &level, bool multiplayer, bool water_pilot, xl_assets::ObjectDependencies &graph)
{
	graph.xl_boss_spawning = true;
	auto blockers = xl_compat::runtime_blockers(level, water_pilot);
	if (!blockers.empty()) throw std::runtime_error(xl_compat::blocker_message(blockers));
	if (multiplayer) throw std::runtime_error("multiplayer: XL synchronization is not implemented; use singleplayer");
	// Both callers checked mission context before reading or translating the level
	if (Current_mission && (Current_mission->alternate_ham_file || (Current_mission->enhanced && Current_mission->enhanced != MISSION_ENHANCED_XL)))
		throw std::runtime_error("custom HAM definitions: XL gameplay-definition support is not verified");
	if (Current_mission && Current_mission->path) {
		const char *descriptor = mission_descriptor_snapshot();
		if (!descriptor) throw std::runtime_error("mission descriptor snapshot unavailable for XL requirement checks");
		std::istringstream input(descriptor);
		bool custom_robots = false, custom_textures = false;
		std::string text;
		while (std::getline(input, text)) {
			text = text.substr(0, text.find(';'));
			const auto equals = text.find('=');
			if (equals == std::string::npos) continue;
			auto key = text.substr(0, equals);
			key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char c) { return std::isspace(c); }), key.end());
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			if (key == "custom_robots" || key == "custom_textures") {
				auto value = text.substr(equals + 1);
				value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); }), value.end());
				std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
				if (value == "yes" || value == "true" || value == "1") {
					if (key == "custom_robots") custom_robots = true;
					else custom_textures = true;
				} else if (value != "no" && value != "false" && value != "0")
					throw std::runtime_error("unrecognized mission resource declaration: " + key + " = " + value);
				continue;
			}
			// Remaining DLE/Hoggle metadata does not select runtime options
			// Multiplayer is gated independently; custom music is not gameplay admission
			const char *metadata[] = { "author", "web_site", "email", "date", "custom_music",
				                       "normal", "anarchy", "robo_anarchy", "coop", "capture_flag", "hoard", "editor", "build_time", "revision", "multi_author", "want_feedback" };
			if (std::find(std::begin(metadata), std::end(metadata), key) != std::end(metadata)) continue;
			if (key != "name" && key != "d2x-name" && key != "type" && key != "num_levels" && key != "num_secrets" && key != "briefing" && key != "ending")
				throw std::runtime_error("unsupported mission descriptor option: " + key);
		}
		// Declarations apply to the mission, not necessarily every individual level
		if (custom_robots && !mission_has_companion(".HXM"))
			throw std::runtime_error("mission declares custom robots, but no level HXM was found; install the complete mission package (other custom definition formats remain unqualified)");
		if (custom_textures && !mission_has_companion(".POG"))
			throw std::runtime_error("mission declares custom textures, but no level POG was found; install the complete mission package (custom PIG replacements remain unqualified)");
	}
	xl_validate_level_graphics(level.palette);
	if (NumTextures <= 404 && std::any_of(level.segments.begin(), level.segments.end(), [&](const xl_compat::Segment &segment) { return (xl_compat::segment_property_bits(level.level_version, segment) & xl_compat::segment_lava) != 0; }))
		throw std::runtime_error("lava volumes require the loaded D2 texture 404 damage definition");
	// Weapons can be collected or restored even when not placed as weapon objects
	for (const auto id : Primary_weapon_to_weapon_info) graph.weapon(id);
	for (const auto id : Secondary_weapon_to_weapon_info) graph.weapon(id);
	for (int id = LASER_ID_L1; id <= LASER_ID_L4; ++id) graph.weapon(id);
	graph.weapon(LASER_ID_L5);
	graph.weapon(LASER_ID_L6);
	graph.weapon(FLARE_ID);
	if (water_pilot) {
		graph.sound(SOUND_SHIP_IN_WATER);
		graph.sound(SOUND_SHIP_IN_WATERFALL);
	}
	if (std::any_of(level.triggers.begin(), level.triggers.end(), [](const xl_compat::Trigger &trigger) { return trigger.type == 14; })) {
		graph.sound(SOUND_SECRET_EXIT);
		graph.clip(VCLIP_PLAYER_APPEARANCE);
	}
	for (const auto &entry : level.object_trigger_records) {
		if (entry.object < 0) continue;
		if (entry.action.type == 26) {
			const auto &owner = level.objects.at(entry.object);
			if (owner.type != OBJ_ROBOT || owner.id < 0 || owner.id >= N_robot_types || !graph.robot_data[owner.id].boss_flag)
				throw std::runtime_error("spawn-bot action requires a robot boss owner: object " + std::to_string(entry.object) + ", robot definition " + std::to_string(owner.id));
			graph.robot(owner.id);
			bool arbitrary = entry.action.targets.empty() || entry.action.targets[0][0] < 0;
			if (!arbitrary) {
				const auto &segment = level.segments.at(entry.action.targets[0][0]);
				const auto &row = level.matcens.at(segment.producer);
				// Even with implicit candidates, a fresh empty producer selects arbitrarily
				// until its normal handler has initialized the third selection word
				arbitrary = !row[0] && !row[1];
			}
			if (arbitrary) {
				int eligible = 0;
				for (int id = 0; id < N_robot_types; ++id) {
					const auto &robot = graph.robot_data[id];
					if (!robot.boss_flag && !robot.companion && robot.score_value >= 700) {
						graph.robot(id);
						++eligible;
					}
				}
				if (!eligible) throw std::runtime_error("spawn-bot action has no eligible arbitrary robot definitions");
			}
			graph.clip(VCLIP_MORPHING_ROBOT);
		}
	}
	for (std::size_t i = 0; i < level.objects.size(); ++i) {
		const auto &obj = level.objects[i];
		// Cooperative starts are removed by singleplayer initialization
		if (obj.type == OBJ_NONE || obj.type == OBJ_COOP) continue;
		try {
			if (obj.type == OBJ_ROBOT) {
				graph.robot(obj.id);
				if (graph.robot_data[obj.id].boss_flag) {
					const bool authored_spawn = std::any_of(level.object_trigger_records.begin(), level.object_trigger_records.end(), [&](const xl_compat::ObjectTrigger &entry) { return entry.object == static_cast<int>(i) && entry.action.type == 26; });
					if (!authored_spawn)
						for (int index = 0, id; (id = xl_boss_spawn_default_type(graph.robot_data[obj.id].boss_flag, index)) >= 0; ++index) graph.robot(id);
					graph.clip(VCLIP_MORPHING_ROBOT);
				}
			}
			if (obj.type == OBJ_POWERUP) graph.powerup(obj.id);
			if (obj.type == OBJ_WEAPON) graph.weapon(obj.id);
			if (obj.type == OBJ_CNTRLCEN) {
				if (obj.id >= Num_reactors || obj.id >= MAX_REACTORS) throw std::runtime_error("required reactor definition is missing");
				graph.weapon(CONTROLCEN_WEAPON_NUM);
			}
			if (obj.contains_count > 0) graph.contents(obj.contains_type, obj.contains_id);
			if (obj.render == RT_POLYOBJ || obj.render == RT_MORPH) {
				const int saved = obj.render_data.at(0);
				if (saved < 0 || std::size_t(saved) >= level.model_names.size()) throw std::runtime_error("model index outside level model table");
				int resolved = -1;
				if (obj.type == OBJ_PLAYER) resolved = Player_ship->model_num;
				else if (obj.type == OBJ_ROBOT) resolved = graph.robot_data[obj.id].model_num;
				else if (obj.type == OBJ_CNTRLCEN) resolved = saved;
				for (int model = 0; resolved < 0 && model < N_polygon_models; ++model)
					if (!d_stricmp(level.model_names[saved].c_str(), Pof_names[model])) {
						resolved = model;
						break;
					}
				if (resolved < 0) throw std::runtime_error("required model is missing: " + level.model_names[saved]);
				graph.model(resolved);
				const int texture = obj.render_data.back();
				if (texture != -1) graph.texture(texture);
			}
			if (obj.render == RT_POWERUP || obj.render == RT_HOSTAGE || obj.render == RT_FIREBALL || obj.render == RT_WEAPON_VCLIP)
				graph.clip(obj.render_data.at(0));
		} catch (const std::exception &error) {
			throw std::runtime_error("object " + std::to_string(i) + ": " + error.what());
		}
	}
	for (const auto &matcen : level.matcens)
		for (unsigned bit = 0; bit < 64; ++bit)
			if (uint32_t(matcen[bit / 32]) & (uint32_t(1) << (bit % 32))) {
				graph.robot(int(bit));
				if (graph.robot_data[bit].boss_flag) throw std::runtime_error("generator-created bosses require dynamic boss lifecycle support");
			}
	if (!level.matcens.empty()) {
		graph.clip(VCLIP_MORPHING_ROBOT);
		graph.clip(VCLIP_PLAYER_HIT);
		graph.sound(SOUND_PLAYER_GOT_HIT);
		graph.sound(SOUND_ROBOT_HIT);
	}
	for (const auto &maker : level.equipment)
		for (unsigned id = 0; id < 50; ++id)
			if (std::uint32_t(maker[id / 32]) & (std::uint32_t(1) << (id % 32))) graph.powerup(static_cast<int>(id));
	if (!level.equipment.empty()) graph.clip(VCLIP_MORPHING_ROBOT);
	for (const auto &segment : level.segments)
		for (const auto &side : segment.sides) {
			if (side.texture >= 0) graph.texture(side.texture);
			if (side.overlay > 0) graph.texture(side.overlay & 0x3fff);
		}
	for (const auto &wall : level.walls) graph.wall_clip(wall.clip);
}
} // namespace

#ifdef __ANDROID__
extern "C" int xl_preview_set_water_test_profile(int enabled)
{
	if (Game_wind) return 0;
	water_test_profile = enabled != 0;
	return 1;
}
extern "C" int xl_preview_set_classic_light_test_profile(int enabled)
{
	if (Game_wind) return 0;
	classic_light_test_profile = enabled != 0;
	if (!classic_light_test_profile) xl_classic_lighting_release();
	return 1;
}
#endif

extern "C" const char *xl_level_rejection(void)
{
	return rejection.c_str();
}

extern "C" int xl_level_supported(const char *filename, int multiplayer)
{
	rejection.clear();
	try {
		validate_mission_context();
		const auto bytes = read_extended_level(filename);
		if (bytes.empty()) {
			if (Current_mission && Current_mission->enhanced == MISSION_ENHANCED_XL)
				throw std::runtime_error("d2x-name mission resources: stock-format levels in XL descriptors are not qualified");
			return 1;
		}
		const bool arena = water_profile(bytes, multiplayer != 0);
		const auto probe = xl_compat::prepare_gameplay_probe(bytes, arena);
		XlPreparedResources resources(filename, &probe.source);
		validate_requirements(probe.source, multiplayer != 0, arena, resources.dependencies());
		return 1;
	} catch (const std::exception &error) {
		rejection = std::string(filename) + "\n" + error.what();
		return 0;
	}
}

extern "C" int xl_test_stop_after_level(void)
{
#ifdef DXX_XL_DEVELOPER_PROBE
	return option("-fixture-path") && option("-xl-test-navigation") && !option("-xl-test-campaign");
#else
	return 0;
#endif
}

extern "C" void xl_classic_lighting_release(void)
{
	if (classic_ambient) {
		dynamic_vertices.clear();
		brightness.clear();
		muzzle_lights.clear();
	}
	classic_ambient.reset();
	std::vector<xl_compat::Rgb>().swap(classic_colors);
}

extern "C" int xl_classic_lighting_requested(void)
{
	return option("-xl-classic-light-preview") || classic_light_test_profile;
}

extern "C" int xl_classic_lighting_prepare(const char *filename)
{
	xl_classic_lighting_release();
	if (!xl_classic_lighting_requested()) return 1;
	if (!Current_mission || Current_mission->descent_version != 1 || (Game_mode & GM_MULTI)) return 1;
	rejection.clear();
	try {
		const auto companions = xl_compat::load_mission_lighting_companions(filename);
		if (!companions.brightness && !companions.colors) return 1;
		if (!companions.brightness || !companions.colors) throw std::runtime_error("classic lighting preview requires both LGT and CLR companions");
		const auto optics = xl_compat::classic_default_lighting_camera();
		const auto sources = xl_compat::compile_classic_geometry_lights(*companions.brightness, *companions.colors);
		auto pending = std::make_unique<xl_compat::ClassicAmbientLighting>(sources, optics);
		auto colors = pending->gather(xl_compat::ClassicAmbientProfile{});
		std::vector<float> level_brightness(colors.size(), 0);
		for (int segment = 0; segment <= Highest_segment_index; ++segment)
			for (int side = 0; side < 6; ++side)
				for (int corner = 0; corner < 4; ++corner) {
					const auto vertex = Segments[segment].verts[Side_to_verts_int[side][corner]];
					level_brightness.at(vertex) = (std::max) (level_brightness.at(vertex), Segments[segment].sides[side].uvls[corner].l / 65536.0f);
				}
		std::vector<xl_compat::DynamicVertexLight> level_dynamic(colors.size());
		classic_ambient = std::move(pending);
		classic_colors = std::move(colors);
		brightness = std::move(level_brightness);
		dynamic_vertices = std::move(level_dynamic);
		return 1;
	} catch (const std::exception &error) {
		xl_classic_lighting_release();
		rejection = std::string("Experimental classic lighting: ") + error.what();
		return 0;
	}
}

extern "C" unsigned xl_classic_lighting_vertex_count(void)
{
	return static_cast<unsigned>(classic_colors.size());
}

extern "C" int xl_classic_legacy_fans_active(void)
{
	return classic_ambient && Game_wind && Current_mission &&
	       Current_mission->descent_version == 1 && !(Game_mode & GM_MULTI);
}

extern "C" int xl_classic_lighting_color(int vertex, float rgb[3])
{
	if (!classic_ambient || !Current_mission || Current_mission->descent_version != 1 || (Game_mode & GM_MULTI) ||
	    !rgb || vertex < 0 || static_cast<std::size_t>(vertex) >= classic_colors.size()) return 0;
	std::copy(classic_colors[vertex].begin(), classic_colors[vertex].end(), rgb);
	return 1;
}

extern "C" void xl_preview_clear_mission(void)
{
	xl_projectile_clear();
	xl_wall_trigger_reset();
	xl_speed_boost_clear();
	xl_spawn_clear();
	xl_fluid_clear();
	xl_blocked_reset();
	xl_repair_clear();
	xl_object_trigger_clear();
	xl_robot_generator_clear();
	xl_boss_spawn_clear();
	xl_equipment_clear();
	xl_classic_lighting_release();
	xl_discard_prepared_resources();
	admitted_pilot = completed_pilot = admitted_water_arena = false;
	activated_triggers.fill(false);
	source.reset();
#ifdef DXX_XL_DESCRIPTOR_TEST
	no_damage_test_source.reset();
#endif
	vertex_calls = 0;
	corona_draws = 0;
	water_draws = water_depth_failures = 0;
	corona_depth_failures = 0;
	corona_classic_requested_draws = 0;
	face_alpha = -1;
	face_water_material = false;
	active_cloak_wall = drawn_cloak_wall = -1;
	drawn_cloak_value = 0;
	cloak_draws = 0;
	drawn_cloak_alpha = 0;
	dynamic_vertices.clear();
	rebuilt_ambient.clear();
	brightness.clear();
	unresolved_emitters = 0;
	muzzle_lights.clear();
	player_thrust = {};
	pending_level.clear();
	loading_wide_walls = false;
	loading_extended_triggers = false;
}

// Method-0 GatherStaticLights when ReadColor did not register baked colors
static void rebuild_xl_ambient()
{
	if (!source || admitted_water_arena || xl_compat::has_registered_colors(*source)) return;
	xl_compat::TextureBrightnessTable table{};
	for (const auto &entry : xl_compat::texture_brightness) table.at(entry.texture) = entry.brightness;
	for (unsigned i = 0; i < table.size(); ++i) {
		int value;
		if (xl_prepared_texture_brightness(i, &value)) table[i] = value;
	}
	xl_compat::TextureRgbTable colors{};
	for (unsigned i = 0; i < colors.size(); ++i)
		for (unsigned channel = 0; channel < 3; ++channel) {
			const auto serialized = source->texture_colors.at(i).rgb[channel];
			// Match ReadColor: floating v15 or integer v17, without narrowing v15 to int32
			const float loaded = source->level_version == 15 ? static_cast<float>(serialized) : static_cast<float>(std::round(serialized * 2147483647.0)) / static_cast<float>(0x7fffffff);
			if (!std::isfinite(loaded)) throw std::runtime_error("XL ambient texture color exceeds finite float range");
			colors[i][channel] = loaded;
		}
	const auto lights = xl_compat::compile_classic_geometry_lights(table, colors);
	const xl_compat::ClassicAmbientLighting ambient(lights, xl_compat::classic_default_lighting_camera());
	rebuilt_ambient = ambient.gather(xl_compat::ClassicAmbientProfile{});
#ifdef DXX_XL_DESCRIPTOR_TEST
	if (const char *path = option("-xl-ambient-report")) {
		std::ostringstream report;
		report << std::setprecision(9) << "{\n  \"vertices\": " << rebuilt_ambient.size() << ",\n  \"sources\": [";
		for (std::size_t i = 0; i < lights.size(); ++i) {
			const auto &entry = lights[i];
			report << (i ? "," : "") << "\n    {\"segment\": " << entry.segment << ", \"side\": " << entry.side
			       << ", \"brightness\": " << entry.material.brightness << ", \"rgb\": ["
			       << entry.material.rgb[0] << ", " << entry.material.rgb[1] << ", " << entry.material.rgb[2] << "]}";
		}
		report << "\n  ],\n  \"colors\": [";
		for (std::size_t i = 0; i < rebuilt_ambient.size(); ++i) {
			const auto &rgb = rebuilt_ambient[i];
			report << (i ? "," : "") << "\n    [" << rgb[0] << ", " << rgb[1] << ", " << rgb[2] << "]";
		}
		report << "\n  ]\n}\n";
		std::ifstream old(path, std::ios::binary);
		const std::string existing{ std::istreambuf_iterator<char>(old), std::istreambuf_iterator<char>() };
		if (existing != report.str()) {
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << report.str();
			if (!output) throw std::runtime_error("Cannot write ambient diagnostic report");
		}
	}
#endif
}

extern "C" unsigned xl_preview_rebuilt_ambient_vertices(void)
{
	return static_cast<unsigned>(rebuilt_ambient.size());
}

extern "C" int xl_preview_load_level(const char *filename)
{
	// A mode change must never leave singleplayer extensions active in multiplayer
	if (Game_mode & GM_MULTI) xl_preview_clear_mission();
	rejection.clear();
#ifdef DXX_XL_DEVELOPER_PROBE
	const bool translated_preview = option("-xl-static-light-preview") && !std::strcmp(filename, "xlprobe.rl2");
#else
	const bool translated_preview = false;
#endif
	int result = 0;
	bool committed = false;
	try {
		validate_mission_context();
		auto bytes = read_extended_level(filename);
		const bool original = !bytes.empty();
		if (!original) {
			if (Current_mission && Current_mission->enhanced == MISSION_ENHANCED_XL)
				throw std::runtime_error("d2x-name mission resources: stock-format levels in XL descriptors are not qualified");
			if (!translated_preview) {
				xl_preview_clear_mission();
				return xl_stock_load_level(filename);
			}
#ifdef DXX_XL_DEVELOPER_PROBE
			if (Game_mode & GM_MULTI) throw std::runtime_error("multiplayer: XL preview synchronization is not implemented");
			const char *path = option("-xl-source");
			if (!path || !*path) throw std::runtime_error("-xl-source is required for the developer preview");
			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream || stream.tellg() < 0 || stream.tellg() > 64 * 1024 * 1024) throw std::runtime_error("cannot read bounded XL source");
			const auto size = stream.tellg();
			stream.seekg(0);
			bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			if (stream.bad() || bytes.size() != std::size_t(size)) throw std::runtime_error("XL source read failed");
#endif
		}
		const bool arena = water_profile(bytes, (Game_mode & GM_MULTI) != 0);
		auto probe = xl_compat::prepare_gameplay_probe(bytes, arena);
		auto resources = std::make_unique<XlPreparedResources>(filename, &probe.source);
		validate_requirements(probe.source, (Game_mode & GM_MULTI) != 0, arena, resources->dependencies());
		if (original) {
			if (!PHYSFS_mkdir("xl_preview_cache")) throw std::runtime_error("cannot create private pilot translation directory");
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> output(PHYSFS_openWrite(translated_path), PHYSFS_close);
			if (!output || PHYSFS_write(output.get(), probe.stock_level.data(), 1, PHYSFS_uint32(probe.stock_level.size())) != PHYSFS_sint64(probe.stock_level.size())) throw std::runtime_error("cannot write private pilot translation");
			if (!PHYSFS_close(output.release())) throw std::runtime_error("cannot close private pilot translation");
		}
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> loaded(PHYSFS_openRead(original ? translated_path : filename), PHYSFS_close);
		if (!loaded || PHYSFS_fileLength(loaded.get()) != PHYSFS_sint64(probe.stock_level.size()))
			throw std::runtime_error("loaded fixture is not the source translation");
		xl_compat::Bytes loaded_bytes(probe.stock_level.size());
		if (PHYSFS_read(loaded.get(), loaded_bytes.data(), 1, PHYSFS_uint32(loaded_bytes.size())) != PHYSFS_sint64(loaded_bytes.size()) || loaded_bytes != probe.stock_level)
			throw std::runtime_error("loaded fixture bytes differ from the source translation");
		loaded.reset();
		// All read-only admission and translated-byte checks precede the native commit
		committed = true;
		xl_preview_clear_mission();
		if (original) pending_level = filename;
		loading_wide_walls = probe.source.walls.size() > DXX_CLASSIC_MAX_WALLS;
		loading_extended_triggers = probe.source.triggers.size() > DXX_CLASSIC_MAX_TRIGGERS;
		result = xl_stock_load_level(filename);
		loading_wide_walls = false;
		loading_extended_triggers = false;
		pending_level.clear();
		if (result) return result;
		auto level = std::make_unique<xl_compat::Inventory>(std::move(probe.source));
		if (level->vertices.size() != std::size_t(Num_vertices) || level->segments.size() != std::size_t(Num_segments))
			throw std::runtime_error("XL source does not match loaded geometry counts");
		for (int i = 0; i < Num_vertices; ++i) {
			const auto &v = level->vertices[i];
			if (v[0] != Vertices[i].x || v[1] != Vertices[i].y || v[2] != Vertices[i].z)
				throw std::runtime_error("XL source does not match loaded vertices");
		}
		for (int i = 0; i < Num_segments; ++i)
			for (unsigned j = 0; j < 8; ++j)
				if (level->segments[i].vertices[j] != Segments[i].verts[j])
					throw std::runtime_error("XL source does not match loaded segment vertices");
		brightness = xl_compat::vertex_brightness(*level);
		dynamic_vertices.resize(level->vertices.size());
		xl_set_prepared_resources(filename, std::move(resources));
		source = std::move(level);
		xl_object_trigger_load(*source);
		xl_robot_generator_load();
		xl_boss_spawn_load();
		xl_equipment_load(*source);
		admitted_water_arena = arena && source->level_version < 21;
		admitted_pilot = original && !admitted_water_arena;
		rebuild_xl_ambient();
		con_printf(CON_NORMAL, admitted_water_arena ? "XL water physics: detected experimental profile, visual parity unqualified\n" : "XL active: detected level requirements and asset dependencies supported\n");
		return result;
	} catch (const std::exception &error) {
		loading_wide_walls = false;
		loading_extended_triggers = false;
		pending_level.clear();
		if (committed) xl_preview_clear_mission();
		rejection = std::string(filename) + "\n" + error.what();
		con_printf(CON_URGENT, "XL level rejected: %s\n", rejection.c_str());
		return 1;
	}
}

#ifdef DXX_XL_DESCRIPTOR_TEST
void xl_test_bind_no_damage(const xl_compat::Inventory &level)
{
	if (Game_mode & GM_MULTI || !((level.level_version == 17 && level.game_version == 36) || (level.level_version == 23 && level.game_version == 40)) ||
	    level.vertices.size() != std::size_t(Num_vertices) || level.segments.size() != std::size_t(Num_segments))
		throw std::runtime_error("no-damage qualification source profile/geometry mismatch");
	for (int i = 0; i < Num_vertices; ++i)
		if (level.vertices[i][0] != Vertices[i].x || level.vertices[i][1] != Vertices[i].y || level.vertices[i][2] != Vertices[i].z)
			throw std::runtime_error("no-damage qualification vertex mismatch");
	for (int i = 0; i < Num_segments; ++i)
		for (unsigned j = 0; j < 8; ++j)
			if (level.segments[i].vertices[j] != Segments[i].verts[j])
				throw std::runtime_error("no-damage qualification segment mismatch");
	no_damage_test_source = std::make_unique<xl_compat::Inventory>(level);
}
#endif

extern "C" int xl_preview_no_damage_segment(int segment)
{
	const auto *level = source.get();
#ifdef DXX_XL_DESCRIPTOR_TEST
	if (no_damage_test_source) level = no_damage_test_source.get();
#endif
	return level && !(Game_mode & GM_MULTI) && segment >= 0 &&
	       std::size_t(segment) < level->segments.size() &&
	       (xl_compat::segment_property_bits(level->level_version, level->segments[segment]) & xl_compat::segment_no_damage);
}

extern "C" int xl_preview_preserve_fusion_queue(void)
{
	// Reference FireGun retains queued fusion across the no-damage exit boundary
	if ((Game_mode & GM_MULTI) || Players[Player_num].primary_weapon != FUSION_INDEX ||
	    Fusion_charge <= 0 || Global_laser_firing_count <= 0) return 0;
	const auto *level = source.get();
#ifdef DXX_XL_DESCRIPTOR_TEST
	if (no_damage_test_source) level = no_damage_test_source.get();
#endif
	return level && std::any_of(level->segments.begin(), level->segments.end(), [&](const xl_compat::Segment &segment) {
		       return (xl_compat::segment_property_bits(level->level_version, segment) & xl_compat::segment_no_damage) != 0;
	       });
}

// Water is immutable authored state reconstructed on every level load/save restore
// Profile: legacy fluid physics enabled, speed scale zero; no general XL admission
extern "C" int xl_preview_water_segment(int segment)
{
	return source && !(Game_mode & GM_MULTI) && segment >= 0 &&
	       std::size_t(segment) < source->segments.size() &&
	       (xl_compat::segment_property_bits(source->level_version, source->segments[segment]) & xl_compat::segment_water);
}

extern "C" int32_t xl_preview_motion_time(int segment, int32_t time)
{
	return xl_preview_water_segment(segment) ? int32_t(int64_t(time) * 75 / 100) : xl_lava_segment(segment) ? int32_t(int64_t(time) * 66 / 100)
	                                                                                                        : time;
}

extern "C" int xl_blocked_segment(int segment)
{
	return source && source->level_version >= 21 && !(Game_mode & GM_MULTI) && segment >= 0 && std::size_t(segment) < source->segments.size() && (source->segments[segment].properties & xl_compat::segment_blocked);
}

extern "C" int xl_source_level_version(void)
{
	return source ? source->level_version : 0;
}
extern "C" int xl_source_game_version(void)
{
	return source ? source->game_version : 0;
}

extern "C" int xl_repair_segment(int segment)
{
	return source && (source->level_version == 23 || (source->level_version == 17 && source->game_version == 36)) && !(Game_mode & GM_MULTI) && segment >= 0 && std::size_t(segment) < source->segments.size() && source->segments[segment].function == 2;
}

extern "C" int xl_blocked_map_segment(int segment)
{
	return xl_blocked_segment(segment) && source->segments[segment].function == 0;
}

extern "C" int xl_lava_segment(int segment)
{
	return source && source->level_version >= 21 && !(Game_mode & GM_MULTI) && segment >= 0 &&
	       std::size_t(segment) < source->segments.size() && (source->segments[segment].properties & xl_compat::segment_lava);
}

extern "C" int xl_preview_water_contact(int object_index, int segment)
{
	if (!xl_preview_water_segment(segment) || object_index < 0 || object_index > Highest_object_index) return 0;
	auto &obj = Objects[object_index];
	const auto &thrust = obj.mtype.phys_info.thrust;
	if (obj.type == OBJ_PLAYER && obj.id == Player_num && (thrust.x || thrust.y || thrust.z)) {
		obj.mtype.phys_info.rotvel.x = (16383 - d_rand()) / 2;
		obj.mtype.phys_info.rotvel.z = (16383 - d_rand()) / 2;
	}
	return 2;
}

extern "C" int xl_preview_water_frame(int object_index)
{
	if (object_index < 0 || object_index > Highest_object_index) return 0;
	auto &obj = Objects[object_index];
	if (obj.type != OBJ_PLAYER || obj.id != Player_num || !xl_preview_water_segment(obj.segnum)) return 0;
	const auto &thrust = obj.mtype.phys_info.thrust;
	if (!thrust.x && !thrust.y && !thrust.z) return 0;
	// Physical turbulence affects orientation: use saved simulation RNG, never FX
	char location[] = "XL water";
	const int divisor = get_seg_masks(&obj.pos, obj.segnum, obj.size, location, __LINE__).sidemask ? 2 : 8;
	obj.mtype.phys_info.rotvel.x = (16383 - d_rand()) / divisor;
	obj.mtype.phys_info.rotvel.z = (16383 - d_rand()) / divisor;
	return 1;
}

// Stock type 2 means texture water; XL also uses it for segment water
extern "C" int xl_preview_water_sound(int object_index, int side, int contact_sound)
{
	if (object_index < 0 || object_index > Highest_object_index) return contact_sound;
	const auto &obj = Objects[object_index];
	if (!xl_preview_water_segment(obj.segnum) || side < 0 || side >= 6) return contact_sound;
	const auto texture = Segments[obj.segnum].sides[side].tmap_num;
	if (TmapInfo[texture].damage || (TmapInfo[texture].flags & TMI_WATER)) return contact_sound;
	const auto &thrust = obj.mtype.phys_info.thrust;
	return thrust.x || thrust.y || thrust.z ? SOUND_SHIP_IN_WATER : -1;
}

extern "C" int xl_preview_fluid_loop(int object_index, int sound, int playing)
{
	if (object_index < 0 || object_index > Highest_object_index) return playing;
	// Preserve the classic one-loop behavior outside water and on classic builds
	const bool fluid = xl_level_extensions_active() || playing == SOUND_SHIP_IN_WATER + 1 || playing == SOUND_LAVAFALL_HISS + 1;
	// Native reset/restore can remove the sound while the frame-local marker survives
	if (fluid && playing && !xl_linked_sound_count(object_index, playing - 1, 0)) playing = 0;
	if (fluid && playing && playing != sound + 1) {
		digi_kill_sound_linked_to_object(object_index);
		playing = 0;
	}
	if (sound >= 0 && !playing) {
		digi_link_sound_to_object3(sound, object_index, 1, F1_0, i2f(256), -1, -1);
		if (!fluid || xl_linked_sound_count(object_index, sound, 0)) playing = sound + 1;
	}
	return playing;
}

extern "C" const char *xl_preview_level_path(const char *filename)
{
	return !pending_level.empty() && pending_level == filename ? translated_path : filename;
}

extern "C" int xl_loading_extended_triggers(void)
{
	return loading_extended_triggers;
}

extern "C" int xl_loading_wide_walls(void)
{
	return loading_wide_walls;
}

extern "C" int xl_preview_pilot_active(void)
{
	return admitted_pilot && source && !(Game_mode & GM_MULTI);
}

extern "C" int xl_runtime_active(void)
{
	// Loaded XL extensions, not a particular mission or build capability
	return (source || classic_ambient) && Game_wind && !(Game_mode & GM_MULTI);
}

extern "C" int xl_level_extensions_active(void)
{
	return source && !(Game_mode & GM_MULTI);
}

extern "C" int xl_speed_boost_destination_valid(const int32_t point[3])
{
	if (!source || !point) return 0;
	const auto matches = [&](const xl_compat::Trigger &action) {
		if (action.type != 15 || action.targets.empty()) return false;
		const auto &target = action.targets[0];
		if (target[0] < 0 || target[0] >= Num_segments || target[1] < 0 || target[1] >= 6) return false;
		const auto side = xl_compat::compile_classic_lighting_side(target[0], target[1]);
		return std::equal(side.center.begin(), side.center.end(), point);
	};
	for (const auto &action : source->triggers)
		if (matches(action)) return 1;
	for (const auto &record : source->object_trigger_records)
		if (matches(record.action)) return 1;
	return 0;
}

extern "C" int xl_speed_boost_segment(int segment)
{
	if (!source || segment < 0 || static_cast<std::size_t>(segment) >= source->segments.size()) return 0;
	return source->segments[segment].function == (source->level_version >= 21 ? 9 : 11);
}

extern "C" int xl_spawn_targets_valid(const xl_spawn_point *points, int count)
{
	if (!source || !points || count != XL_SPAWN_SLOTS) return 0;
	const auto matches = [&](const xl_compat::Trigger &action) {
		if (action.type != 28 || action.targets.empty()) return false;
		for (int slot = 0; slot < count; ++slot) {
			const auto &target = action.targets[slot % action.targets.size()];
			if (points[slot].segment != target[0] || points[slot].side != target[1]) return false;
		}
		return true;
	};
	for (const auto &action : source->triggers)
		if (matches(action)) return 1;
	for (const auto &record : source->object_trigger_records)
		if (matches(record.action)) return 1;
	return 0;
}

extern "C" int xl_level_segment_group(int segment)
{
	return source && segment >= 0 && static_cast<std::size_t>(segment) < source->segments.size() ? source->segments[segment].group : 0;
}

extern "C" int xl_level_trigger_state_required(void)
{
	// Surface scraping carries a simulation clock even in otherwise ordinary XL geometry
	return source != nullptr;
}

extern "C" uint64_t xl_level_trigger_identity(void)
{
	if (!source) return 0;
	std::uint64_t identity = UINT64_C(14695981039346656037);
	const auto field = [&](int value) {
		for (unsigned shift = 0; shift < 32; shift += 8) {
			identity ^= (std::uint32_t(value) >> shift) & 255;
			identity *= UINT64_C(1099511628211);
		}
	};
	field(source->level_version);
	field(source->game_version);
	const bool boosts = std::any_of(source->triggers.begin(), source->triggers.end(), [](const xl_compat::Trigger &action) { return action.type == 15; }) ||
	                    std::any_of(source->object_trigger_records.begin(), source->object_trigger_records.end(), [](const xl_compat::ObjectTrigger &record) { return record.action.type == 15; });
	const bool spawns = std::any_of(source->triggers.begin(), source->triggers.end(), [](const xl_compat::Trigger &action) { return action.type == 28; }) ||
	                    std::any_of(source->object_trigger_records.begin(), source->object_trigger_records.end(), [](const xl_compat::ObjectTrigger &record) { return record.action.type == 28; });
	if (boosts || spawns || xl_boss_spawn_count() || !source->equipment.empty()) {
		field(xl_boss_spawn_count());
		field(static_cast<int>(source->vertices.size()));
		field(static_cast<int>(source->segments.size()));
		field(static_cast<int>(source->walls.size()));
		field(static_cast<int>(source->objects.size()));
		for (const auto &vertex : source->vertices)
			for (int value : vertex) field(value);
		for (const auto &segment : source->segments) {
			field(segment.group);
			if (boosts || spawns) field(segment.function);
			for (int child : segment.children) field(child);
			for (int vertex : segment.vertices) field(vertex);
		}
		for (const auto &wall : source->walls)
			for (int value : { wall.segment, wall.side, wall.type, wall.flags, wall.state }) field(value);
		for (std::size_t i = 0; i < source->objects.size(); ++i) {
			const auto &object = source->objects[i];
			if (object.type == OBJ_ROBOT || !source->equipment.empty()) {
				field(static_cast<int>(i));
				if (!source->equipment.empty()) field(object.type);
				field(object.id);
				field(object.segment);
				for (int value : object.position) field(value);
			}
		}
	}
	if (!source->equipment.empty()) {
		field(static_cast<int>(source->equipment.size()));
		for (const auto &row : source->equipment)
			for (int value : row) field(value);
		for (const auto &segment : source->segments) {
			field(segment.function);
			field(segment.producer);
			field(segment.owner);
		}
	}
	if (!source->matcens.empty()) {
		field(static_cast<int>(source->matcens.size()));
		for (const auto &row : source->matcens)
			for (int value : row) field(value);
		for (const auto &segment : source->segments) {
			field(segment.function);
			field(segment.producer);
		}
		for (const auto &object : source->objects)
			if (object.type == OBJ_ROBOT) field(object.id);
	}
	field(static_cast<int>(source->triggers.size()));
	for (const auto &trigger : source->triggers) {
		for (int value : { trigger.type, trigger.flags, trigger.value, trigger.time, static_cast<int>(trigger.targets.size()) }) field(value);
		for (const auto &target : trigger.targets) {
			field(target[0]);
			field(target[1]);
		}
	}
	if (source->object_triggers) {
		field(source->object_triggers);
		for (const auto &entry : source->object_trigger_records) {
			field(entry.object);
			const auto &action = entry.action;
			for (int value : { action.type, action.flags, action.value, action.time, static_cast<int>(action.targets.size()) }) field(value);
			for (const auto &target : action.targets) {
				field(target[0]);
				field(target[1]);
			}
			if (entry.object >= 0) {
				field(source->objects[entry.object].type);
				field(source->objects[entry.object].id);
			}
		}
	}
	return identity;
}

extern "C" void xl_preview_complete_pilot(void)
{
	if (xl_preview_pilot_active()) completed_pilot = true;
}

extern "C" int xl_preview_pilot_completed(void)
{
	return completed_pilot;
}

extern "C" void xl_preview_trigger_result(int trigger, int shot, int result)
{
	if (!xl_preview_pilot_active() || trigger < 0 || trigger >= Num_triggers || trigger >= int(activated_triggers.size())) return;
	if (result && !(Triggers[trigger].type == TT_EXIT && completed_pilot)) return;
	if (activated_triggers[trigger]) return;
	activated_triggers[trigger] = true;
	con_printf(CON_DEBUG, "XL pilot trigger=%d type=%d shot=%d links=%d\n", trigger, Triggers[trigger].type, shot, Triggers[trigger].num_links);
}

extern "C" uint64_t xl_preview_trigger_mask(void)
{
	uint64_t mask = 0;
	for (unsigned index = 0; index < 64; ++index)
		if (activated_triggers[index]) mask |= uint64_t(1) << index;
	return mask;
}

extern "C" int xl_preview_trigger_activated(int index)
{
	return index >= 0 && index < int(activated_triggers.size()) && activated_triggers[index];
}

extern "C" int xl_preview_texture_emitter(int segment_number, int side_number, xl_texture_emitter *emitter)
{
	if (emitter) *emitter = {};
	// The water pilot retains its previously qualified presentation profile
	if (!emitter || !source || admitted_water_arena || Game_mode & GM_MULTI || segment_number < 0 || segment_number >= Num_segments || side_number < 0 || side_number >= 6) return 0;
	const auto &side = Segments[segment_number].sides[side_number];
	if (side.wall_num >= 0) {
		if (side.wall_num >= Num_walls) return 0;
		const auto &w = Walls[side.wall_num];
		if (w.type == WALL_BLASTABLE || w.type == WALL_DOOR || w.type == WALL_OPEN || w.type == WALL_CLOAKED || w.flags & (WALL_BLASTED | WALL_ILLUSION_OFF)) return 0;
	}
	if (Light_subtracted[segment_number] & (1 << side_number)) return 0;
	const auto light = [](int texture) {
		int brightness = 0;
		if (xl_prepared_texture_brightness(texture, &brightness)) return int32_t(brightness);
		for (const auto &entry : xl_compat::texture_brightness)
			if (entry.texture == texture) return entry.brightness;
		return int32_t(0);
	};
	int texture = side.tmap_num2 & 0x3fff;
	float intensity = 1;
	if (!texture || light(texture) < F1_0 / 8) {
		texture = side.tmap_num;
		if (!light(texture)) return 0;
		intensity = 0.5f;
	}
	// Pinned XL FaceHasCorona exclusions and bright strip-light scale
	switch (texture) {
		case 0:
		case 6:
		case 302:
		case 348:
		case 349:
		case 353:
		case 356:
		case 357:
		case 358:
		case 359:
		case 360:
		case 361:
		case 432:
		case 433:
		case 434: return 0;
		case 404:
		case 405:
		case 406:
		case 407:
		case 408:
		case 409: intensity *= 2; break;
		default: break;
	}
	if (texture < 0 || texture >= NumTextures || std::size_t(texture) >= source->texture_colors.size()) return 0;
	const auto bitmap = Textures[texture].index;
	if (bitmap >= Num_bitmap_files) return 0;
	const char *name = piggy_game_bitmap_name(&GameBitmaps[bitmap]);
	if (!name || std::strstr(name, "metl") || std::strstr(name, "rock") || std::strstr(name, "water")) return 0;
	emitter->texture = texture;
	emitter->intensity = intensity;
	for (unsigned c = 0; c < 3; ++c) emitter->rgb[c] = float(source->texture_colors[texture].rgb[c]);
	return 1;
}

extern "C" int xl_preview_corona_geometry(int segment_number, int side_number, const int32_t eye[3], int eye_segment,
                                          int32_t center[3], int32_t *radius, float *facing)
{
	if (!source || segment_number < 0 || segment_number >= Num_segments || side_number < 0 || side_number >= 6 || eye_segment < 0 || eye_segment >= Num_segments) return 0;
	auto &seg = Segments[segment_number];
	if (!(WALL_IS_DOORWAY(&seg, side_number) & WID_RENDER_FLAG)) return 0;
	vms_vector point{}, inside{}, observer{ eye[0], eye[1], eye[2] }, direction{}, normals[2];
	compute_center_point_on_side(&point, &seg, side_number);
	compute_segment_center(&inside, &seg);
#ifdef COMPACT_SEGS
	get_side_normals(&seg, side_number, &normals[0], &normals[1]);
#else
	normals[0] = seg.sides[side_number].normals[0];
	normals[1] = seg.sides[side_number].normals[1];
#endif
	vm_vec_normalized_dir(&direction, &observer, &point);
	const fix cosine = (std::max) (vm_vec_dot(&direction, &normals[0]), vm_vec_dot(&direction, &normals[1]));
	if (cosine <= 0) return 0;
	// Place the sprite just inside the surface to avoid depth fighting
	point.x += (inside.x - point.x) / 256;
	point.y += (inside.y - point.y) / 256;
	point.z += (inside.z - point.z) / 256;
	fvi_query query{};
	query.p0 = &observer;
	query.p1 = &point;
	query.startseg = eye_segment;
	query.thisobjnum = -1;
	query.flags = FQ_TRANSWALL;
	fvi_info hit{};
	if (find_vector_intersection(&query, &hit) != HIT_NONE) return 0;
	int vertices[4];
	get_side_verts(vertices, segment_number, side_number);
	fix extent = (std::numeric_limits<fix>::max)();
	for (unsigned i = 0; i < 4; ++i)
		extent = (std::min) (extent, vm_vec_dist(&Vertices[vertices[i]], &Vertices[vertices[(i + 1) % 4]]));
	if (extent <= 0) return 0;
	center[0] = point.x;
	center[1] = point.y;
	center[2] = point.z;
	*radius = extent / 2;
	*facing = std::sqrt(float(cosine) / F1_0);
	return 1;
}

extern "C" void xl_preview_render_emitters(const short *segments, int count, const int32_t eye[3], int eye_segment)
{
	if (!source || Game_mode & GM_MULTI) return;
	for (int i = 0; i < count; ++i) {
		for (int side = 0; side < 6; ++side) {
			xl_texture_emitter emitter{};
			if (!xl_preview_texture_emitter(segments[i], side, &emitter)) continue;
			int32_t center[3], radius;
			float facing;
			if (!xl_preview_corona_geometry(segments[i], side, eye, eye_segment, center, &radius, &facing)) continue;
			ogl_record_xl_corona_depth();
			corona_draws += ogl_draw_xl_corona(center, radius, emitter.rgb, emitter.intensity * facing * 0.5f);
		}
	}
}

extern "C" uint64_t xl_preview_corona_draws(void)
{
	return corona_draws;
}

extern "C" void xl_preview_record_corona_depth(int enabled, int classic_requested)
{
	if (!enabled) ++corona_depth_failures;
	if (classic_requested) ++corona_classic_requested_draws;
}

extern "C" uint64_t xl_preview_corona_classic_requested_draws(void)
{
	return corona_classic_requested_draws;
}

extern "C" uint64_t xl_preview_corona_depth_failures(void)
{
	return corona_depth_failures;
}

extern "C" void xl_preview_vertex(int vertex, int32_t light, int32_t *r, int32_t *g, int32_t *b)
{
	xl_compat::Rgb classic_color;
	const bool classic = xl_classic_lighting_color(vertex, classic_color.data()) != 0;
	if (!classic && (!source || vertex < 0 || std::size_t(vertex) >= source->vertex_colors.size())) return;
	try {
		xl_compat::VertexLighting input;
		if (classic) input.ambient = classic_color;
		else if (!rebuilt_ambient.empty()) input.ambient = rebuilt_ambient.at(vertex);
		else
			for (unsigned i = 0; i < 3; ++i) input.ambient[i] = float(source->vertex_colors[vertex].rgb[i]);
		input.static_light = light;
		const auto &dynamic = dynamic_vertices.at(vertex);
		input.dynamic_light = dynamic.intensity;
		input.dynamic_color = dynamic.color;
		input.has_dynamic_color = dynamic.has_color;
		input.extra_game_info = true;
		input.vertex_brightness = brightness.at(vertex);
		input.flashing = Control_center_destroyed || Seismic_tremor_magnitude;
		input.flash_scale = flash_scale;
		// XL FlashFrame keeps the reactor branch at least one fixed-point unit
		if (Control_center_destroyed && !Seismic_tremor_magnitude && input.flash_scale == 0)
			input.flash_scale = 1;
		const auto shaded = face_water_material ? xl_compat::shade_water_vertex(input, true) : xl_compat::shade_vertex(input);
		*r = int32_t(shaded.rgb[0] * 65536);
		*g = int32_t(shaded.rgb[1] * 65536);
		*b = int32_t(shaded.rgb[2] * 65536);
		++vertex_calls;
	} catch (const std::exception &error) {
		Error("XL preview lighting failed: %s", error.what());
	}
}

extern "C" uint64_t xl_preview_vertex_calls(void)
{
	return vertex_calls;
}

extern "C" int xl_preview_wall_is_doorway(int segment_index, int side)
{
	segment *seg = &Segments[segment_index];
	const int stock = xl_stock_wall_is_doorway(seg, side);
	if (!source || option("-xl-preview-stock-materials")) return stock;
	const int doorway = xl_wall_doorway(-1, segment_index, side);
	return doorway >= 0 ? doorway : stock;
}

extern "C" void xl_preview_begin_face(int segment_number, int side)
{
	face_alpha = -1;
	face_water_material = false;
	if (!source || option("-xl-preview-stock-materials") || segment_number < 0 || segment_number >= Num_segments || side < 0 || side >= 6) return;
	const int index = Segments[segment_number].sides[side].wall_num;
	if (source->level_version >= 21) {
		xl_compat::WaterFace face;
		const auto &original = source->segments[segment_number];
		const int child = Segments[segment_number].children[side];
		face.water = xl_preview_water_segment(segment_number) != 0;
		face.has_child = child >= 0 && child < Num_segments;
		face.child_water = face.has_child && xl_preview_water_segment(child);
		face.function = original.function;
		face.child_function = face.has_child ? source->segments[child].function : 0;
		face.has_wall = index >= 0 && index < Num_walls;
		// Match XL HasTexture, including its child-or-wall condition
		if (face.has_child || face.has_wall) {
			const auto &s = Segments[segment_number].sides[side];
			const auto fluid = [](int texture) { return texture >= 0 && texture < NumTextures && (TmapInfo[texture].damage || (TmapInfo[texture].flags & TMI_WATER)); };
			face.fluid_texture = fluid(s.tmap_num) || (s.tmap_num2 && fluid(s.tmap_num2 & 0x3fff));
		}
		face_water_material = xl_compat::water_face_colored(face);
	}
	if (index < 0 || index >= Num_walls) return;
	const wall &w = Walls[index];
	// Cloaked flat walls are handled before the textured-face hook
	if (w.type == WALL_CLOAKED) return;
	const int c = w.cloak_value;
	face_alpha = c > 0 && c < xl_fade_levels ? float(xl_fade_levels - c) / xl_fade_levels : 1.0f;
}

extern "C" void xl_preview_end_face(void)
{
	face_alpha = -1;
	face_water_material = false;
	active_cloak_wall = -1;
}

extern "C" int xl_preview_water_portal(int segment_number, int side, int32_t points[4][3], float colors[4][4])
{
	if (!source || source->level_version < 21 || Game_mode & GM_MULTI || segment_number < 0 || segment_number >= Num_segments || side < 0 || side >= 6) return 0;
	const auto &segment = Segments[segment_number];
	const auto &face = segment.sides[side];
	// Wall-backed faces keep their normal visibility/material dispatch
	// -1 is a textured outer wall; -2 is the open external-exit boundary
	if (segment.children[side] == -1 || face.wall_num >= 0) return 0;
	xl_preview_begin_face(segment_number, side);
	if (!face_water_material) {
		xl_preview_end_face();
		return 0;
	}
	int vertices[4];
	get_side_verts(vertices, segment_number, side);
	for (int i = 0; i < 4; ++i) {
		// A rotated fan preserves the compiled 1-3 diagonal
		const int corner = (i + (face.type == SIDE_IS_TRI_13 ? 1 : 0)) % 4;
		const int vertex = vertices[corner];
		const auto &p = Vertices[vertex];
		points[i][0] = p.x;
		points[i][1] = p.y;
		points[i][2] = p.z;
		int32_t r = 0, g = 0, b = 0;
		// XL SetupFace leaves bTextured=true for a no-wall colored face
		xl_preview_vertex(vertex, face.uvls[corner].l, &r, &g, &b);
		colors[i][0] = float(r) / F1_0;
		colors[i][1] = float(g) / F1_0;
		colors[i][2] = float(b) / F1_0;
		colors[i][3] = 0.333f;
	}
	xl_preview_end_face();
	return 1;
}

extern "C" void xl_preview_draw_water_portal(int segment_number, int side)
{
	int32_t points[4][3];
	float colors[4][4];
	if (xl_preview_water_portal(segment_number, side, points, colors)) ogl_draw_xl_water_portal(points, colors);
}

extern "C" void xl_preview_record_water_draw(int depth_enabled)
{
	++water_draws;
	if (!depth_enabled) ++water_depth_failures;
}

extern "C" uint64_t xl_preview_water_draws(void)
{
	return water_draws;
}

extern "C" uint64_t xl_preview_water_depth_failures(void)
{
	return water_depth_failures;
}

extern "C" float xl_preview_alpha(float stock_alpha)
{
	return face_alpha < 0 ? stock_alpha : face_alpha;
}

extern "C" int xl_preview_begin_cloak_face(int segment_number, int side)
{
	if (!xl_level_extensions_active() || segment_number < 0 || segment_number >= Num_segments || side < 0 || side >= 6) return 0;
	const int index = Segments[segment_number].sides[side].wall_num;
	if (index < 0 || index >= Num_walls) return 0;
	const auto &wall = Walls[index];
	if (wall.type != WALL_CLOAKED && wall.state != WALL_DOOR_CLOAKING && wall.state != WALL_DOOR_DECLOAKING) return 0;
	// Method-0 WallAlpha uses a flat face throughout both transition halves
	const int cloak = wall.cloak_value;
	face_alpha = cloak >= xl_fade_levels ? 0.0f : 1.0f - float(cloak) / xl_fade_levels;
	active_cloak_wall = index;
	return 1;
}

extern "C" void xl_preview_record_cloak_draw(float alpha)
{
	if (active_cloak_wall < 0 || active_cloak_wall >= Num_walls) return;
	++cloak_draws;
	drawn_cloak_wall = active_cloak_wall;
	drawn_cloak_value = Walls[active_cloak_wall].cloak_value;
	drawn_cloak_alpha = alpha;
}
extern "C" uint64_t xl_preview_cloak_draws(void)
{
	return cloak_draws;
}
extern "C" int xl_preview_cloak_draw_wall(void)
{
	return drawn_cloak_wall;
}
extern "C" int xl_preview_cloak_draw_value(void)
{
	return drawn_cloak_value;
}
extern "C" float xl_preview_cloak_draw_alpha(void)
{
	return drawn_cloak_alpha;
}

extern "C" unsigned xl_preview_unresolved_emitters(void)
{
	return unresolved_emitters;
}

extern "C" void xl_preview_reset_player_thrust(void)
{
	player_thrust = {};
}

extern "C" void xl_preview_player_thrust(int player, int32_t x, int32_t y, int32_t z)
{
	if (player != Player_num) return;
	// Reference profile: ship 0 speed=1, damage model off gives 2*DriveDamage=1
	player_thrust = Player_is_dead ? vms_vector{} : vms_vector{ x, y, z };
}

extern "C" void xl_preview_muzzle_snapshot(int64_t current_time)
{
	muzzle_lights.clear();
	if (!source && !classic_ambient) return;
	// Called with the stock loop's timestamp, before its one queue-expiration pass
	// Pinned XL also narrows elapsed time to int16, including its wrap behavior
	constexpr fix duration = F1_0 / 3, scale = 3 * F1_0 / duration;
	for (const auto &flash : Muzzle_data) {
		if (!flash.create_time) continue;
		const auto elapsed = int16_t(uint16_t(uint64_t(current_time) - uint64_t(flash.create_time)));
		if (elapsed < duration) muzzle_lights.push_back({ flash.pos, (duration - elapsed) * scale });
	}
}

extern "C" void xl_preview_dynamic_frame(void)
{
	if ((!source && !classic_ambient) || (classic_ambient && (Game_mode & GM_MULTI))) return;
	try {
		std::fill(dynamic_vertices.begin(), dynamic_vertices.end(), xl_compat::DynamicVertexLight{});
		unresolved_emitters = 0;
		if (!Do_dynamic_light) return;
		std::vector<bool> visible(Num_vertices, false);
		for (int i = 0; i < N_render_segs; ++i) {
			const int seg = Render_list[i];
			if (seg < 0 || seg >= Num_segments) continue;
			for (const int vertex : Segments[seg].verts) visible.at(vertex) = true;
		}
		// XL casts non-object flashes before object sources, without source color
		for (const auto &flash : muzzle_lights) {
			for (int vertex = 0; vertex < Num_vertices; ++vertex) {
				if (!visible[vertex]) continue;
				const fix distance = vm_vec_dist(&flash.position, &Vertices[vertex]);
				if (distance >= int64_t(flash.intensity) * 64) continue;
				const fix bounded = (std::max) (distance, fix(MIN_LIGHT_DIST));
				dynamic_vertices[vertex].add(int32_t(int64_t(flash.intensity) * F1_0 / bounded), nullptr, false);
			}
		}
		// First live emitter slice; unresolved sources remain explicit, never certified
		for (int i = 0; i <= Highest_object_index; ++i) {
			const object &obj = Objects[i];
			fix intensity = 0;
			xl_compat::Rgb color{ 1, 1, 1 };
			bool has_color = false;
			bool force_color = false;
			bool headlight = false;
			switch (obj.type) {
				case OBJ_ROBOT:
					intensity = Robot_info[obj.id].lightcast ? (Robot_info[obj.id].lighting ? Robot_info[obj.id].lighting : F1_0) : 0;
					has_color = true;
					break;
				case OBJ_DEBRIS: intensity = F1_0 / 4; break;
				case OBJ_LIGHT: intensity = obj.ctype.light_info.intensity; break;
				// Saved reference profile disables powerup lights; runtime parity pending
				case OBJ_POWERUP: break;
				case OBJ_WEAPON:
					if (obj.id >= N_weapon_types) throw std::runtime_error("XL weapon light ID invalid");
					if ((Game_mode & GM_MULTI) && obj.id == OMEGA_ID) {
						++unresolved_emitters;
						break;
					}
					intensity = Weapon_info[obj.id].light;
					if (obj.id == FLARE_ID) intensity = xl_compat::flare_intensity(intensity, obj.lifeleft, unsigned(d_rand_fx()));
					// CWeaponData::Create initializes all non-missile colors to white
					if (missile_light(obj.id)) color = { 1, 0.3f, 0 };
					has_color = force_color = true;
					break;
				case OBJ_MARKER:
					intensity = 8 * std::abs(F1_0 / 2 - (obj.lifeleft & 0xffff));
					color = { 0.1f, 1, 0.1f };
					has_color = true;
					// Stock emission already extends lifetime by F1_0, preserving low bits
					// This hook runs after it; do not mutate lifetime a second time
					break;
				case OBJ_PLAYER:
					if (Game_mode & (GM_MULTI | GM_HOARD)) {
						++unresolved_emitters;
						break;
					}
					if (obj.id != Player_num) {
						++unresolved_emitters;
						break;
					}
					headlight = (Players[obj.id].flags & (PLAYER_FLAGS_HEADLIGHT | PLAYER_FLAGS_HEADLIGHT_ON)) == (PLAYER_FLAGS_HEADLIGHT | PLAYER_FLAGS_HEADLIGHT_ON);
					intensity = headlight ? 10 * F1_0 : (std::max) (vm_vec_mag(&player_thrust) / 4, fix(2 * F1_0)) + F1_0 / 2;
					force_color = headlight;
					has_color = true;
					break;
				case OBJ_FIREBALL: {
					if (obj.id == 255 || obj.id >= Num_vclips) break;
					const auto &clip = Vclip[obj.id];
					if (clip.num_frames <= 0 || clip.num_frames > VCLIP_MAX_FRAMES || clip.play_time <= 0)
						throw std::runtime_error("XL explosion animation invalid");
					color = {};
					for (int frame = 0; frame < clip.num_frames; ++frame) {
						const auto frame_color = animation_frame_color(clip.frames[frame]);
						for (unsigned c = 0; c < 3; ++c) color[c] += frame_color[c];
					}
					for (auto &c : color) c /= clip.num_frames;
					const float peak = *std::max_element(color.begin(), color.end());
					if (peak > 0)
						for (auto &c : color) c /= peak;
					intensity = obj.lifeleft < 4 * F1_0 ? fixmul(fixdiv(obj.lifeleft, clip.play_time), clip.light_value) : clip.light_value;
					has_color = force_color = true;
					break;
				}
				default: break;
			}
			if (!intensity) continue;
			const auto radius = std::int64_t(intensity) * 64;
			const auto magnitude = radius < 0 ? -radius : radius;
			const bool local = magnitude <= F1_0 * 8 || obj.type == OBJ_MARKER;
			if (obj.segnum < 0 || obj.segnum >= Num_segments) throw std::runtime_error("XL emitter segment invalid");
			for (int vertex = 0; vertex < Num_vertices; ++vertex) {
				if (local) {
					const auto &vertices = Segments[obj.segnum].verts;
					if (std::find(std::begin(vertices), std::end(vertices), vertex) == std::end(vertices)) continue;
				} else if (!visible[vertex]) continue;
				fix distance = vm_vec_dist(&obj.pos, &Vertices[vertex]);
				if (local) distance = fixmul(distance / 4, distance / 4);
				if ((distance >> (headlight ? 3 : 0)) >= magnitude) continue;
				distance = (std::max) (distance, fix(MIN_LIGHT_DIST));
				auto contribution = std::int64_t(intensity) * F1_0 / distance;
				if (headlight) {
					vms_vector direction;
					vm_vec_sub(&direction, &Vertices[vertex], &obj.pos);
					vm_vec_normalize(&direction);
					const fix dot = vm_vec_dot(&direction, &obj.orient.fvec);
					contribution = dot < F1_0 / 2
					                   ? fixdiv(intensity, fixmul(10 * F1_0, distance))
					                   : fixmul(fixmul(dot, dot), intensity) / 8;
				}
				if (contribution < (std::numeric_limits<int32_t>::min)() || contribution > (std::numeric_limits<int32_t>::max)())
					throw std::runtime_error("XL dynamic contribution overflow");
				dynamic_vertices[vertex].add(int32_t(contribution), has_color ? &color : nullptr, force_color);
			}
		}
	} catch (const std::exception &error) {
		Error("XL dynamic preview failed: %s", error.what());
	}
}
