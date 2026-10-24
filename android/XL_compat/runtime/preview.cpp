// Explicit static-light preview only; no production mission admission
#include "preview.h"
#include "../probe.h"
#include "../lighting.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include "picosha2.h"
extern "C" {
#include "inferno.h"
#include "segment.h"
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
#include "gameseq.h"
#include "switch.h"
#include "dxxerror.h"
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
bool admitted_pilot = false, completed_pilot = false;
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
} // namespace

extern "C" int xl_preview_load_level(const char *filename)
{
	admitted_pilot = completed_pilot = false;
	activated_triggers = 0;
	source.reset();
	vertex_calls = 0;
	face_alpha = -1;
	dynamic_vertices.clear();
	brightness.clear();
	unresolved_emitters = 0;
	muzzle_lights.clear();
	player_thrust = {};
	pending_level.clear();
	const char *base = std::strrchr(filename, '/');
	base = base ? base + 1 : filename;
	const bool pilot_mission = Current_mission && (!std::strcmp(Current_mission_filename, "d2-xl") ||
	                                               (option("-fixture-path") && !std::strcmp(Current_mission_filename, "xlprobe")));
	const bool original = option("-xl-pilot") && pilot_mission && !std::strcmp(base, "d2leva-1.rl2");
	int result = 0;
	if (!original) {
		result = xl_stock_load_level(filename);
		if (result || !option("-xl-static-light-preview") || std::strcmp(base, "xlprobe.rl2")) return result;
	}
	try {
		xl_compat::Bytes bytes;
		if (original) {
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(PHYSFS_openRead(filename), PHYSFS_close);
			if (!input || PHYSFS_fileLength(input.get()) != 149235) throw std::runtime_error("pilot source size differs from audited level");
			bytes.resize(149235);
			if (PHYSFS_read(input.get(), bytes.data(), 1, PHYSFS_uint32(bytes.size())) != PHYSFS_sint64(bytes.size())) throw std::runtime_error("pilot source read failed");
			if (picosha2::hash256_hex_string(bytes) != "5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2") throw std::runtime_error("pilot source hash differs from audited level");
		} else {
			const char *path = option("-xl-source");
			if (!path || !*path) throw std::runtime_error("-xl-source is required");
			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream || stream.tellg() < 0 || stream.tellg() > 64 * 1024 * 1024)
				throw std::runtime_error("cannot read bounded XL source");
			const auto size = stream.tellg();
			stream.seekg(0);
			bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			if (stream.bad() || bytes.size() != std::size_t(size)) throw std::runtime_error("XL source read failed");
		}
		auto probe = xl_compat::prepare_gameplay_probe(bytes);
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
		admitted_pilot = original;
		con_printf(CON_URGENT, "XL PREVIEW: dynamic lighting and face effects are unverified; not production support\n");
		return result;
	} catch (const std::exception &error) {
		pending_level.clear();
		con_printf(CON_URGENT, "XL preview rejected: %s\n", error.what());
		return 1;
	}
}

extern "C" const char *xl_preview_level_path(const char *filename)
{
	return !pending_level.empty() && pending_level == filename ? translated_path : filename;
}

extern "C" int xl_preview_pilot_active(void)
{
	return admitted_pilot && source && Current_level_num == 1 && !(Game_mode & GM_MULTI);
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
