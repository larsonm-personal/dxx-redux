// Detected XL requirements and runtime integration; developer previews remain explicit
#include "preview.h"
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
#include <cctype>
#include "picosha2.h"
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
uint64_t vertex_calls = 0;
uint64_t corona_draws = 0;
uint64_t corona_depth_failures = 0;
uint64_t corona_classic_requested_draws = 0;
float face_alpha = -1;
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
bool admitted_pilot = false, completed_pilot = false, admitted_water_arena = false;
std::string rejection;
uint64_t activated_triggers = 0;
constexpr const char *translated_path = "xl_preview_cache/pilot.rl2";
// XL palette.h uses 31; stock GR_FADE_LEVELS is 34 and cannot be reused
constexpr int xl_fade_levels = 31;
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

// This exception preserves the separately tested developer arena, not general water admission
bool water_arena(const char *filename, const xl_compat::Bytes &bytes, bool multiplayer)
{
	const char *base = std::strrchr(filename, '/');
	base = base ? base + 1 : filename;
	if (!option("-xl-water-arena") || !Current_mission || std::strcmp(Current_mission_filename, "rtarena") || std::strcmp(base, "rtarena.rl2")) return false;
	if (multiplayer) throw std::runtime_error("water arena multiplayer is unqualified");
	if (bytes.size() != 80598 || picosha2::hash256_hex_string(bytes) != "5d456eb26f603d942e565a7944c3837741613dccbed6573323bcd56874133392")
		throw std::runtime_error("water arena source differs from audited level");
	return true;
}

void validate_requirements(const xl_compat::Inventory &level, const char *filename, bool multiplayer)
{
	const auto blockers = xl_compat::runtime_blockers(level);
	if (!blockers.empty()) throw std::runtime_error(xl_compat::blocker_message(blockers));
	if (multiplayer) throw std::runtime_error("multiplayer: XL synchronization is not implemented; use singleplayer");
	if (Current_mission && Current_mission->enhanced)
		throw std::runtime_error("custom HAM definitions: XL gameplay-definition support is not verified");
	if (Current_mission && Current_mission->path) {
		std::string descriptor = std::string(Current_mission->path) + ".mn2";
		if (!PHYSFSX_exists(descriptor.c_str(), 1)) descriptor = std::string(MISSION_DIR) + descriptor;
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(PHYSFSX_openReadBuffered(descriptor.c_str()), PHYSFS_close);
		if (!input) throw std::runtime_error("mission descriptor unavailable for XL requirement checks");
		char line[1024];
		while (PHYSFSX_fgets(line, sizeof(line), input.get())) {
			std::string text = line;
			text = text.substr(0, text.find(';'));
			const auto equals = text.find('=');
			if (equals == std::string::npos) continue;
			auto key = text.substr(0, equals);
			key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char c) { return std::isspace(c); }), key.end());
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			if (key != "name" && key != "d2x-name" && key != "type" && key != "num_levels" && key != "num_secrets" && key != "briefing" && key != "ending")
				throw std::runtime_error("unsupported mission descriptor option: " + key);
		}
	}
	std::string hxm = filename;
	const auto dot = hxm.find_last_of('.');
	if (dot != std::string::npos) hxm.replace(dot, std::string::npos, ".hxm");
	if (PHYSFSX_exists(hxm.c_str(), 1) || PHYSFSX_exists((std::string(MISSION_DIR) + hxm).c_str(), 1))
		throw std::runtime_error("level-specific HXM definitions: XL override support is not verified");
	if (level.palette.empty() || !PHYSFSX_exists(level.palette.c_str(), 1))
		throw std::runtime_error("required palette is missing: " + level.palette);
	std::string pig = level.palette;
	const auto extension = pig.find_last_of('.');
	if (extension != std::string::npos) pig.replace(extension, std::string::npos, ".pig");
	if (!PHYSFSX_exists(pig.c_str(), 1)) throw std::runtime_error("required texture archive is missing: " + pig);
	xl_assets::ObjectDependencies graph;
	for (std::size_t i = 0; i < level.objects.size(); ++i) {
		const auto &obj = level.objects[i];
		// Cooperative starts are removed by singleplayer initialization
		if (obj.type == OBJ_NONE || obj.type == OBJ_COOP) continue;
		try {
			if (obj.type == OBJ_ROBOT) graph.robot(obj.id);
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
				else if (obj.type == OBJ_ROBOT) resolved = Robot_info[obj.id].model_num;
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
			if (uint32_t(matcen[bit / 32]) & (uint32_t(1) << (bit % 32))) graph.robot(int(bit));
	for (const auto &segment : level.segments)
		for (const auto &side : segment.sides) {
			if (side.texture >= 0) graph.texture(side.texture);
			if (side.overlay > 0) graph.texture(side.overlay & 0x3fff);
		}
	for (const auto &wall : level.walls) graph.wall_clip(wall.clip);
}
} // namespace

extern "C" const char *xl_level_rejection(void)
{
	return rejection.c_str();
}

extern "C" int xl_level_supported(const char *filename, int multiplayer)
{
	rejection.clear();
	try {
		const auto bytes = read_extended_level(filename);
		if (bytes.empty()) return 1;
		const bool arena = water_arena(filename, bytes, multiplayer != 0);
		const auto probe = xl_compat::prepare_gameplay_probe(bytes, arena);
		if (!arena) validate_requirements(probe.source, filename, multiplayer != 0);
		return 1;
	} catch (const std::exception &error) {
		rejection = std::string(filename) + "\n" + error.what();
		return 0;
	}
}

extern "C" int xl_test_stop_after_level(void)
{
	return option("-fixture-path") && option("-xl-test-navigation");
}

extern "C" int xl_preview_load_level(const char *filename)
{
	admitted_pilot = completed_pilot = admitted_water_arena = false;
	activated_triggers = 0;
	source.reset();
	vertex_calls = 0;
	corona_draws = 0;
	corona_depth_failures = 0;
	corona_classic_requested_draws = 0;
	face_alpha = -1;
	dynamic_vertices.clear();
	brightness.clear();
	unresolved_emitters = 0;
	muzzle_lights.clear();
	player_thrust = {};
	pending_level.clear();
	rejection.clear();
	const bool translated_preview = option("-xl-static-light-preview") && !std::strcmp(filename, "xlprobe.rl2");
	int result = 0;
	try {
		auto bytes = read_extended_level(filename);
		const bool original = !bytes.empty();
		if (!original) {
			if (!translated_preview) return xl_stock_load_level(filename);
			if (Game_mode & GM_MULTI) throw std::runtime_error("multiplayer: XL preview synchronization is not implemented");
			const char *path = option("-xl-source");
			if (!path || !*path) throw std::runtime_error("-xl-source is required for the developer preview");
			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream || stream.tellg() < 0 || stream.tellg() > 64 * 1024 * 1024) throw std::runtime_error("cannot read bounded XL source");
			const auto size = stream.tellg();
			stream.seekg(0);
			bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			if (stream.bad() || bytes.size() != std::size_t(size)) throw std::runtime_error("XL source read failed");
			result = xl_stock_load_level(filename);
			if (result) return result;
		}
		const bool arena = water_arena(filename, bytes, (Game_mode & GM_MULTI) != 0);
		auto probe = xl_compat::prepare_gameplay_probe(bytes, arena);
		if (!arena) validate_requirements(probe.source, filename, (Game_mode & GM_MULTI) != 0);
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
		if (original) {
			pending_level = filename;
			result = xl_stock_load_level(filename);
			pending_level.clear();
			if (result) return result;
		}
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
		source = std::move(level);
		admitted_pilot = original && !arena;
		admitted_water_arena = arena;
		con_printf(CON_NORMAL, arena ? "XL water arena: bounded developer pilot, visual parity unqualified\n" : "XL active: detected level requirements and asset dependencies supported\n");
		return result;
	} catch (const std::exception &error) {
		pending_level.clear();
		rejection = std::string(filename) + "\n" + error.what();
		con_printf(CON_URGENT, "XL level rejected: %s\n", rejection.c_str());
		return 1;
	}
}

// Water is immutable authored state reconstructed on every level load/save restore
// Profile: legacy fluid physics enabled, speed scale zero; no general XL admission
extern "C" int xl_preview_water_segment(int segment)
{
	return source && !(Game_mode & GM_MULTI) && segment >= 0 &&
	       std::size_t(segment) < source->segments.size() && source->segments[segment].function == 7;
}

extern "C" int32_t xl_preview_motion_time(int segment, int32_t time)
{
	return xl_preview_water_segment(segment) ? int32_t(int64_t(time) * 75 / 100) : time;
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
	if ((xl_preview_water_segment(Objects[object_index].segnum) || playing == SOUND_SHIP_IN_WATER + 1) &&
	    playing && playing != sound + 1) {
		digi_kill_sound_linked_to_object(object_index);
		playing = 0;
	}
	if (sound >= 0 && !playing) {
		digi_link_sound_to_object3(sound, object_index, 1, F1_0, i2f(256), -1, -1);
		playing = sound + 1;
	}
	return playing;
}

extern "C" const char *xl_preview_level_path(const char *filename)
{
	return !pending_level.empty() && pending_level == filename ? translated_path : filename;
}

extern "C" int xl_preview_pilot_active(void)
{
	return admitted_pilot && source && !(Game_mode & GM_MULTI);
}

extern "C" int xl_runtime_active(void)
{
	// Loaded XL extensions, not a particular mission or build capability
	return source && Game_wind && !(Game_mode & GM_MULTI);
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
	if (!xl_preview_pilot_active() || trigger < 0 || trigger >= Num_triggers || trigger >= 64) return;
	if (result && !(Triggers[trigger].type == TT_EXIT && completed_pilot)) return;
	const uint64_t bit = uint64_t(1) << trigger;
	if (activated_triggers & bit) return;
	activated_triggers |= bit;
	con_printf(CON_DEBUG, "XL pilot trigger=%d type=%d shot=%d links=%d\n", trigger, Triggers[trigger].type, shot, Triggers[trigger].num_links);
}

extern "C" uint64_t xl_preview_trigger_mask(void)
{
	return activated_triggers;
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
	if (!source || vertex < 0 || std::size_t(vertex) >= source->vertex_colors.size()) return;
	try {
		xl_compat::VertexLighting input;
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
		const auto shaded = xl_compat::shade_vertex(input);
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
	const int index = seg->sides[side].wall_num;
	if (index < 0 || index >= Num_walls) return stock;
	const wall &w = Walls[index];
	// CWall::IsPassable: illusion/blastable use <31 including zero;
	// other ordinary walls require a nonzero authored cloak value
	const bool transparent = (w.type == WALL_ILLUSION || w.type == WALL_BLASTABLE)
	                             ? w.cloak_value < xl_fade_levels
	                             : w.cloak_value > 0 && w.cloak_value < xl_fade_levels;
	return transparent ? stock | WID_RENDPAST_FLAG : stock;
}

extern "C" void xl_preview_begin_face(int segment_number, int side)
{
	face_alpha = -1;
	if (!source || option("-xl-preview-stock-materials") || segment_number < 0 || segment_number >= Num_segments || side < 0 || side >= 6) return;
	const int index = Segments[segment_number].sides[side].wall_num;
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
}

extern "C" float xl_preview_alpha(float stock_alpha)
{
	return face_alpha < 0 ? stock_alpha : face_alpha;
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
	if (!source) return;
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
	if (!source) return;
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
