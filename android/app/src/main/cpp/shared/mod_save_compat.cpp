/* Android presentation compatibility, independent of installed/enabled UI rows */
#include <nlohmann/json.hpp>
#include "mod_save_compat.h"
#include "input_demo_codec.h"
#include "mod_save_supported_data.h"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "bm.h"
#include "effects.h"
#include "wall.h"
#include "textures.h"
#include "gameseq.h"
#include "mission.h"
#include "multi.h"
#include "physfsx.h"
#include "android_log.h"
#include "android_save_meta.h"
#include "android_crash_handler.h"
#include "state_checkpoint.h"
#include "coop/coop_save_format.h"
#include "input_demo_replay.h"
#include "hudmsg.h"
#include "newmenu.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif
}

using nlohmann::json;
namespace
{
constexpr unsigned max_block = 256 * 1024;
constexpr char tag[8] = { 'D', 'X', 'M', 'O', 'D', 'S', '0', '1' };
const json &supported()
{
	static const json value = json::parse(supported_mod_save_data);
	return value;
}
json definitions, level_sources, saved;
json extra_ham = json::object();
PHYSFS_sint64 effect_offset;
PHYSFS_sint64 trailer_end;
std::string status = "none", reason;
int adapted;
bool exact_only, legacy, legacy_uud, changed, world_adapted;
bool error_pending;
std::set<std::pair<int, int>> monitor_surfaces;

void check(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
std::string digest(const std::string &data)
{
	std::string result, error;
	check(input_demo_sha256_hex(reinterpret_cast<const unsigned char *>(data.data()), data.size(), &result, &error), "Cannot hash mod definitions");
	return result;
}
std::string source_hash(const std::string &path)
{
	auto *file = PHYSFSX_openReadBuffered(path.c_str());
	if (!file) return "missing";
	picosha2::hash256_one_by_one hash;
	unsigned char buffer[65536];
	PHYSFS_sint64 count;
	while ((count = PHYSFS_readBytes(file, buffer, sizeof(buffer))) > 0) hash.process(buffer, buffer + count);
	const bool closed = PHYSFS_close(file);
	const bool ok = count == 0 && closed;
	check(ok, "Cannot read save asset source");
	hash.finish();
	return picosha2::get_hash_hex_string(hash);
}
json read_patch(const std::string &path)
{
	auto *file = PHYSFSX_openReadBuffered(path.c_str());
	if (!file) return nullptr;
	const auto length = PHYSFS_fileLength(file);
	if (length < 0 || length > 4 * 1024 * 1024) {
		PHYSFS_close(file);
		throw std::runtime_error("Mod patch is too large");
	}
	std::string text(static_cast<size_t>(length), '\0');
	const bool ok = PHYSFS_readBytes(file, text.data(), length) == length;
	PHYSFS_close(file);
	check(ok, "Cannot read mod patch");
	return json::parse(text);
}
bool audited_value(const std::string &path, const json &value)
{
	if (value.is_object()) {
		for (auto it = value.begin(); it != value.end(); ++it)
			if (!audited_value(path + "/" + it.key(), it.value())) return false;
		return !value.empty();
	}
	const auto it = supported().at("ham").find(path);
	return it != supported().at("ham").end() && std::find(it->begin(), it->end(), value) != it->end();
}
bool d1_content()
{
#ifdef DXX_BUILD_DESCENT_II
	return Current_mission && Current_mission->descent_version == 1;
#else
	return true;
#endif
}
json patch_sources()
{
	json result = json::object();
	const std::vector<std::string> paths = d1_content()
	                                           ? std::vector<std::string>{ "patches/d1/properties_patch.rfc6902.json", "patches/d1/level_surface_patch.rfc6902.json" }
	                                           : std::vector<std::string>{ "patches/d2/ham_patch.rfc6902.json" };
	for (const auto &path : paths) {
		std::string actual = path;
#ifdef DXX_BUILD_DESCENT_II
		if (d1_content()) actual = "d1-mods/" + path;
#endif
		const auto patch = read_patch(actual);
		bool audited = patch.is_null();
		const std::string hash = patch.is_null() ? "missing" : digest(patch.dump());
		if (patch.is_array()) {
			if (d1_content()) audited = supported().at("d1").value(path, "") == hash;
			else {
				audited = true;
				for (const auto &op : patch) {
					const auto kind = op.value("op", "");
					if ((kind != "test" && kind != "replace" && kind != "add") ||
					    !op.contains("value") || !audited_value(op.value("path", ""), op.at("value"))) audited = false;
				}
			}
		}
		result[path] = { { "hash", hash }, { "audited", audited } };
	}
	return result;
}
json effect_definition(const eclip &ec)
{
	json row = { ec.vc.num_frames, ec.vc.frame_time, ec.vc.play_time, ec.vc.flags, ec.vc.sound_num };
	for (const auto frame : ec.vc.frames) row.push_back(frame.index);
	for (int value : { ec.vc.light_value, int(ec.changing_wall_texture), int(ec.changing_object_texture), ec.flags & ~(EF_ONE_SHOT | EF_STOPPED),
	                   ec.crit_clip, ec.dest_bm_num, ec.dest_vclip, ec.dest_eclip, ec.dest_size, ec.sound_num }) row.push_back(value);
	return row;
}
json wall_definition(const wclip &wc)
{
	json row = { wc.play_time, int(wc.num_frames), int(wc.flags) };
	for (const auto frame : wc.frames) row.push_back(frame);
	return row;
}
bool known_effect(int index, const json &row)
{
	if (d1_content()) return false;
	const auto it = supported().at("effects").find(std::to_string(index));
	return it != supported().at("effects").end() && std::find(it->begin(), it->end(), row) != it->end();
}
const json &current_effect(int index)
{
	return definitions.at("effects").at(index);
}
bool repaired_monitor(const json &source)
{
	return !d1_content() && source.at("effects").size() > 40 &&
	       known_effect(40, source.at("effects").at(40)) && source.at("effects").at(40).at(0) == 3;
}
int texture_target(int value)
{
	if (value == 910 && !d1_content() && NumTextures <= 910 &&
	    (legacy ? legacy_uud : repaired_monitor(saved)) && known_effect(25, current_effect(25)) &&
	    known_effect(40, current_effect(40))) return 373;
	return value;
}
bool reject(const std::string &message)
{
	status = "incompatible";
	reason = message.substr(0, 240);
	debug_log_force(DLOG_GAME, "Mod save incompatible: %s", reason.c_str());
#ifdef __ANDROID__
	android_restore_phase(reason.c_str());
#endif
	return false;
}
void write_u32(rewind_file *file, uint32_t value)
{
	unsigned char bytes[4] = { static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24) };
	check(rewind_file_write(file, bytes, 1, 4) == 4, "Cannot write mod save footer");
}
uint32_t read_u32(rewind_file *file)
{
	unsigned char bytes[4];
	check(rewind_file_read(file, bytes, 1, 4) == 4, "Truncated mod save footer");
	return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}
json read_block(rewind_file *file)
{
	const auto position = rewind_file_tell(file), length = rewind_file_length(file);
	PHYSFS_sint64 end = length;
	android_save_meta_footer footer;
	if (length >= sizeof(footer) && rewind_file_seek(file, length - sizeof(footer)) &&
	    rewind_file_read(file, &footer, sizeof(footer), 1) == 1 && footer.tag == ANDROID_SAVE_META_TAG) {
		check(footer.trailer_bytes >= sizeof(footer) && footer.trailer_bytes <= length, "Invalid save metadata length");
		end -= footer.trailer_bytes;
	}
	json result;
	coop_save_footer coop_footer;
	if (end >= sizeof(coop_footer) && rewind_file_seek(file, end - sizeof(coop_footer)) &&
	    rewind_file_read(file, &coop_footer, sizeof(coop_footer), 1) == 1 && coop_footer.tag == COOP_SAVE_FOOTER_TAG) {
		check(coop_footer.payload_size <= end - sizeof(coop_footer), "Invalid coop save boundary");
		end -= sizeof(coop_footer) + coop_footer.payload_size;
	}
	char marker[8];
	if (end >= 80 && rewind_file_seek(file, end - 8) && rewind_file_read(file, marker, 1, 8) == 8 && !std::memcmp(marker, tag, 8)) {
		check(rewind_file_seek(file, end - 16), "Invalid mod save footer");
		const auto size = read_u32(file), schema = read_u32(file);
		check(schema == 1 && size <= max_block && size <= end - 80, "Invalid mod save block size or schema");
		check(rewind_file_seek(file, end - 80 - size), "Invalid mod save block offset");
		std::string payload(size, '\0'), expected(64, '\0');
		check(rewind_file_read(file, payload.data(), 1, size) == size && rewind_file_read(file, expected.data(), 1, 64) == 64, "Truncated mod save block");
		check(digest(payload) == expected, "Mod save block checksum mismatch");
		result = json::parse(payload);
		check(result.at("native_end").get<PHYSFS_sint64>() == end - 80 - size, "Invalid mod save native boundary");
		trailer_end = end;
	}
	check(rewind_file_seek(file, position), "Cannot restore save read position");
	return result;
}
int word(rewind_file *file, int swap)
{
	int32_t value;
	check(rewind_file_read(file, &value, 4, 1) == 1, "Truncated effect runtime record");
	return swap ? SWAPINT(value) : value;
}
struct record {
	int effect, time, frame, flags, segment, side, texture;
};
std::vector<record> read_effects(rewind_file *file, int swap, fix64 &loop_time, int highest_segment, bool source_only = false)
{
	uint32_t low = word(file, swap), high = word(file, swap);
	loop_time = static_cast<fix64>((uint64_t(high) << 32) | low);
	const int count = word(file, swap);
	check(count >= 0 && count <= MAX_EFFECTS, "Invalid effect runtime count");
	std::vector<record> result;
	std::set<int> seen, targets;
	for (int index = 0; index < count; ++index) {
		record r = { word(file, swap), word(file, swap), word(file, swap), word(file, swap), word(file, swap), word(file, swap), word(file, swap) };
		try {
			check(r.effect >= 0 && r.effect < MAX_EFFECTS && seen.insert(r.effect).second && r.time >= 0 && r.frame >= 0 && !(r.flags & ~(EF_ONE_SHOT | EF_STOPPED)), "Invalid or duplicate effect runtime record");
			json source;
			if (!legacy) {
				check(r.effect < int(saved.at("effects").size()), "Effect is absent from saved source definitions");
				source = saved.at("effects").at(r.effect);
			} else if (r.effect < Num_effects) source = current_effect(r.effect);
			// Released UUD2 saves predate definition metadata
			if (legacy && !exact_only && !d1_content() && (r.effect == 40 || r.effect == 105 || r.effect == 106) &&
			    (r.effect >= Num_effects || (known_effect(r.effect, source) && source.at(0).get<int>() < 1))) {
				const auto &variants = supported().at("effects").at(std::to_string(r.effect));
				for (const auto &variant : variants)
					if (variant.at(0).get<int>() > 0) source = variant;
				legacy_uud = true;
			}
			check(source.is_array() && source.size() == 45, "Missing source effect definition");
			const int frames = source.at(0), frame_time = source.at(1);
			const bool inactive = frames <= 0 && r.frame == 0 && r.time == 0 && !r.flags && r.segment == -1;
			check(inactive || (frames > 0 && frames <= VCLIP_MAX_FRAMES && r.frame < frames && frame_time > 0), "Saved effect frame is outside its source clip");
			check(r.segment == -1 || (r.segment >= 0 && r.segment <= highest_segment && r.side >= 0 && r.side < 6 && r.texture > 0), "Invalid effect segment or side");
			check(!(r.flags & EF_ONE_SHOT) || r.segment != -1, "One-shot effect has no world target");
			if (source_only) {
				check(r.segment == -1 || r.texture < saved.at("textures").get<int>(), "One-shot target is outside saved source textures");
				result.push_back(r);
				continue;
			}
			const bool different = r.effect >= Num_effects || source != current_effect(r.effect);
			if (different) {
				check(!exact_only && known_effect(r.effect, source) &&
				          (r.effect >= Num_effects || known_effect(r.effect, current_effect(r.effect))),
				      "Effect definitions changed outside supported presentation mods");
				++adapted;
				if (r.effect == 40 && (r.flags & EF_ONE_SHOT) && current_effect(40).at(0).get<int>() <= 0) {
					check(known_effect(38, current_effect(38)), "Missing stock destroyed-monitor effect");
					r.effect = 38;
					r.texture = texture_target(r.texture);
					monitor_surfaces.insert({ r.segment, r.side });
				} else if (r.effect >= Num_effects || current_effect(r.effect).at(0).get<int>() <= 0) {
					check(!r.flags && r.segment == -1, "Removed effect still owns active world state");
					continue;
				}
				if (r.flags & (EF_ONE_SHOT | EF_STOPPED)) {
					const auto &target = Effects[r.effect];
					const int64_t old_duration = int64_t(frames) * frame_time, new_duration = int64_t(target.vc.num_frames) * target.vc.frame_time;
					check(old_duration > 0 && new_duration > 0 && new_duration <= INT32_MAX, "Invalid animation duration");
					const int64_t elapsed = std::clamp(int64_t(r.frame + 1) * frame_time - r.time, int64_t(0), old_duration - 1);
					const auto phase = elapsed * new_duration / old_duration;
					r.frame = int(phase / target.vc.frame_time);
					r.time = target.vc.frame_time - int(phase % target.vc.frame_time);
				} else {
					effect_get_loop_state(&Effects[r.effect], loop_time, &r.frame, &r.time);
				}
				if (r.segment == -1) r.texture = current_effect(r.effect).at(40);
			}
			if (r.segment != -1) {
				r.texture = texture_target(r.texture);
				check(r.texture > 0 && r.texture < NumTextures, "One-shot requires missing mod textures");
			}
			if (!targets.insert(r.effect).second) {
				auto previous = std::find_if(result.begin(), result.end(), [&](const record &row) { return row.effect == r.effect; });
				check(r.effect == 38 && previous != result.end(), "Two saved effects map to the same animation");
				if (r.flags & EF_ONE_SHOT) {
					check(!previous->flags && previous->segment == -1, "Conflicting monitor one-shots");
					*previous = r;
					continue;
				}
				check(!r.flags && r.segment == -1 && (previous->flags & EF_ONE_SHOT), "Conflicting monitor overrides");
				continue;
			}
			result.push_back(r);
		} catch (const std::exception &error) {
			debug_log_force(DLOG_GAME, "Mod save effect rejected: record=%d effect=%d frame=%d time=%d flags=%d segment=%d side=%d texture=%d live_count=%d live_frames=%d reason=%s",
			                index, r.effect, r.frame, r.time, r.flags, r.segment, r.side, r.texture, Num_effects,
			                r.effect >= 0 && r.effect < Num_effects ? Effects[r.effect].vc.num_frames : -1, error.what());
			throw;
		}
	}
	check(!legacy_uud || (seen.count(40) && seen.count(105) && seen.count(106)) || !monitor_surfaces.empty(),
	      "Legacy save has ambiguous mod effects; re-enable original mods and re-save");
	return result;
}
} // namespace

void mod_save_capture_definitions(void)
{
	try {
		extra_ham = json::object();
		definitions = { { "effects", json::array() }, { "walls", json::array() }, { "textures", NumTextures }, { "patches", patch_sources() } };
		for (int i = 0; i < Num_effects; ++i) definitions["effects"].push_back(effect_definition(Effects[i]));
		for (int i = 0; i < Num_wall_anims; ++i) definitions["walls"].push_back(wall_definition(WallAnims[i]));
	} catch (const std::exception &error) {
		definitions = nullptr;
		reject(error.what());
	}
}
void mod_save_capture_level(const char *filename)
{
	try {
		level_sources = json::object();
		std::string base = filename ? filename : "";
		check(!base.empty(), "Missing level source");
		std::vector<std::string> paths = { base };
		const auto dot = base.find_last_of('.');
		if (dot != std::string::npos)
			for (const char *extension : { ".hxm", ".pog", ".dtx" }) paths.push_back(base.substr(0, dot) + extension);
		paths.push_back(d1_content() ? "descent.pig" : "descent2.ham");
		for (const auto &path : paths) level_sources[path] = source_hash(path);
	} catch (const std::exception &error) {
		level_sources = nullptr;
		reject(error.what());
	}
}
void mod_save_capture_extra_ham(const char *filename)
{
	try {
		extra_ham[filename] = source_hash(filename);
	} catch (const std::exception &error) {
		extra_ham = nullptr;
		reject(error.what());
	}
}
void mod_save_effect_offset(rewind_file *file)
{
	effect_offset = rewind_file_tell(file);
}
int mod_save_write(rewind_file *file)
{
	// Rewind is frequent and cannot change assets; retain its compact exact layout
	if (rewind_file_is_memory(file)) {
#ifdef __ANDROID__
		if (!state_checkpoint_writes_file()) return 1;
#else
		return 1;
#endif
	}
	try {
		check(definitions.is_object() && level_sources.is_object() && extra_ham.is_object(), "No source definitions for gameplay save");
		auto block = definitions;
		block["schema"] = 1;
		block["sources"] = level_sources;
		block["extra_ham"] = extra_ham;
		block["level"] = Current_level_num;
		block["highest_segment"] = Highest_segment_index;
		block["effect_offset"] = effect_offset;
		block["native_end"] = rewind_file_tell(file);
		std::set<int> used_textures, used_walls, object_textures;
		for (int i = 0; i <= Highest_segment_index; ++i)
			for (const auto &side : Segments[i].sides) {
				used_textures.insert(side.tmap_num);
				if (side.tmap_num2) used_textures.insert(static_cast<unsigned short>(side.tmap_num2) & 0x3fff);
			}
		for (int i = 0; i < Num_walls; ++i)
			if (Walls[i].clip_num >= 0) used_walls.insert(Walls[i].clip_num);
		block["used_textures"] = used_textures;
		block["used_walls"] = used_walls;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && (Objects[i].render_type == RT_POLYOBJ || Objects[i].render_type == RT_MORPH) && Objects[i].rtype.pobj_info.tmap_override >= 0)
				object_textures.insert(Objects[i].rtype.pobj_info.tmap_override);
		block["object_textures"] = object_textures;
		block["monitor_one_shot"] = !d1_content() && Num_effects > 40 && (Effects[40].flags & EF_ONE_SHOT) != 0;
		const auto payload = block.dump(), hash = digest(payload);
		check(payload.size() <= max_block && rewind_file_write(file, payload.data(), 1, payload.size()) == PHYSFS_sint64(payload.size()) &&
		          rewind_file_write(file, hash.data(), 1, hash.size()) == PHYSFS_sint64(hash.size()),
		      "Cannot write mod save definitions");
		write_u32(file, unsigned(payload.size()));
		write_u32(file, 1);
		check(rewind_file_write(file, tag, 1, sizeof(tag)) == sizeof(tag), "Cannot finish mod save definitions");
		return 1;
	} catch (const std::exception &error) {
		return reject(error.what());
	}
}
int mod_save_prepare(rewind_file *file, int level, int version, int swap)
{
	saved = nullptr;
	legacy = true;
	legacy_uud = false;
	changed = false;
	world_adapted = false;
	adapted = 0;
	trailer_end = -1;
	status = "exact";
	reason.clear();
	monitor_surfaces.clear();
	exact_only = (Game_mode & GM_MULTI) || rewind_file_is_memory(file) || input_demo_replay_is_loaded();
	try {
		saved = read_block(file);
		legacy = saved.is_null();
		const int first_version =
#ifdef DXX_BUILD_DESCENT_II
		    43;
#else
		    20;
#endif
		check(!legacy || rewind_file_is_memory(file) || version < first_version, "Missing mod source definitions in save");
		if (legacy) return 1;
		// D1-in-D2 publishes its own source identity at level entry
		if (!d1_content()) check(saved.at("extra_ham") == extra_ham, "Mission robot or weapon definitions changed; restore the original gameplay assets");
		check(saved.at("schema") == 1 && saved.at("level") == level && saved.at("effects").is_array() &&
		          saved.at("effects").size() <= MAX_EFFECTS && saved.at("walls").size() <= MAX_WALL_ANIMS,
		      "Invalid saved definition tables");
		for (const auto &row : saved.at("effects")) {
			check(row.is_array() && row.size() == 45, "Invalid source effect layout");
			for (const auto &value : row) check(value.is_number_integer() && value.get<int64_t>() >= INT32_MIN && value.get<int64_t>() <= INT32_MAX, "Invalid source effect field");
			check(row.at(0).get<int>() >= -1 && row.at(0).get<int>() <= VCLIP_MAX_FRAMES && row.at(1).get<int>() >= 0, "Invalid source effect timing");
		}
		check(saved.at("sources").is_object() && saved.at("sources").size() <= 8, "Invalid saved asset sources");
		for (auto it = saved.at("sources").begin(); it != saved.at("sources").end(); ++it)
			check(it.key().find("..") == std::string::npos && it.key().find(':') == std::string::npos && source_hash(it.key()) == it.value(), "Mission or gameplay source assets are missing or changed");
		const auto current = patch_sources();
		check(saved.at("patches").size() == current.size(), "Save content mode changed");
		for (auto it = current.begin(); it != current.end(); ++it) {
			const auto &old = saved.at("patches").at(it.key());
			if (old.at("hash") != it.value().at("hash")) {
				check(!exact_only && old.at("audited") == true && it.value().at("audited") == true, "Restore requires the original gameplay mods");
				changed = true;
			}
		}
		// D1 publication is deferred until level entry in the D2 engine
		// Its supported patch changes timing only; source hashes prove table identity
		const bool deferred_d1 = d1_content();
		const int texture_count = deferred_d1 ? saved.at("textures").get<int>() : NumTextures;
		for (int texture : saved.at("object_textures"))
			check(texture >= 0 && texture < texture_count, "Saved object requires extra mod textures; enable its texture pack");
		for (int texture : saved.at("used_textures")) {
			if (texture == 911 && !exact_only && repaired_monitor(saved) && saved.value("monitor_one_shot", false)) continue;
			check(texture_target(texture) >= 0 && texture_target(texture) < texture_count, "Save uses extra UUD2 textures; enable UUD2 Texture Fixes");
		}
		for (int clip : saved.at("used_walls"))
			check(clip >= 0 && clip < (deferred_d1 ? int(saved.at("walls").size()) : Num_wall_anims), "Save uses extra mod doors; enable its texture pack");
		const auto offset = saved.at("effect_offset").get<PHYSFS_sint64>(), end = saved.at("native_end").get<PHYSFS_sint64>();
		const int highest = saved.at("highest_segment");
		check(highest >= 0 && highest < MAX_SEGMENTS && offset >= 0 && offset <= end - 12, "Invalid saved effect boundary");
		const auto position = rewind_file_tell(file);
		check(rewind_file_seek(file, offset), "Cannot seek saved effects");
		fix64 time;
		read_effects(file, swap, time, highest, deferred_d1);
		check(rewind_file_tell(file) <= end && rewind_file_seek(file, position), "Effect records exceed native save boundary");
		return 1;
	} catch (const std::exception &error) {
		return reject(error.what());
	}
}
void mod_save_skip_trailer(rewind_file *file)
{
	if (!legacy && rewind_file_tell(file) == saved.at("native_end").get<PHYSFS_sint64>()) rewind_file_seek(file, trailer_end);
}
int mod_save_check_companion(const char *filename)
{
	auto *raw = PHYSFSX_openReadBuffered(filename);
	if (!raw) return !PHYSFS_exists(filename) || reject("Cannot read secret companion");
	rewind_file file;
	rewind_file_init_physfs(&file, raw);
	const auto primary = saved;
	const auto primary_end = trailer_end;
	const auto primary_monitors = monitor_surfaces;
	const bool primary_legacy = legacy, primary_legacy_uud = legacy_uud, primary_changed = changed, primary_exact = exact_only;
	const int primary_adapted = adapted;
	bool ok = false;
	try {
		const auto companion = read_block(&file);
		if (companion.is_null()) {
			check(!changed && !adapted, "Re-enable the original mods and re-save this legacy secret companion");
			ok = true;
		} else {
			check(rewind_file_seek(&file, 4), "Missing companion header");
			int version = word(&file, 0), swap = 0;
			if (version & 0xffff0000) {
				version = SWAPINT(version);
				swap = 1;
			}
			ok = mod_save_prepare(&file, companion.at("level"), version, swap);
		}
	} catch (const std::exception &error) {
		reject(error.what());
	}
	rewind_file_close(&file);
	saved = primary;
	trailer_end = primary_end;
	monitor_surfaces = primary_monitors;
	legacy = primary_legacy;
	legacy_uud = primary_legacy_uud;
	changed = primary_changed;
	exact_only = primary_exact;
	adapted = primary_adapted;
	return ok;
}
int mod_save_validate_effects(rewind_file *file, int swap)
{
	try {
		fix64 time;
		adapted = 0;
		monitor_surfaces.clear();
		read_effects(file, swap, time, Highest_segment_index);
		return 1;
	} catch (const std::exception &error) {
		return reject(error.what());
	}
}
void mod_save_read_effects(rewind_file *file, int swap, int apply)
{
	try {
		fix64 time;
		const int previous = adapted;
		const auto records = read_effects(file, swap, time, Highest_segment_index);
		adapted = previous;
		if (!apply) return;
		reset_special_effects_to_time(time);
		for (const auto &r : records) {
			auto &ec = Effects[r.effect];
			ec.time_left = r.time;
			ec.frame_count = r.frame;
			ec.flags = (ec.flags & ~(EF_ONE_SHOT | EF_STOPPED)) | r.flags;
			ec.segnum = r.segment;
			ec.sidenum = r.side;
			ec.dest_bm_num = r.texture;
		}
		for (int i = 0; i < Num_effects; ++i) effect_apply_bitmap_state(i);
	} catch (const std::exception &error) {
		reject(error.what());
	}
}
int mod_save_adapt_world(void)
{
	if (world_adapted) return 1;
	try {
		// Validate every mapping before changing any live surface or door clock
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && (Objects[i].render_type == RT_POLYOBJ || Objects[i].render_type == RT_MORPH))
				check(Objects[i].rtype.pobj_info.tmap_override >= -1 && Objects[i].rtype.pobj_info.tmap_override < NumTextures, "Saved object requires missing mod textures");
		std::vector<std::pair<short *, short>> textures;
		for (int i = 0; i <= Highest_segment_index; ++i)
			for (int side_index = 0; side_index < 6; ++side_index) {
				auto &side = Segments[i].sides[side_index];
				check(!monitor_surfaces.count({ i, side_index }) || (static_cast<unsigned short>(side.tmap_num2) & 0x3fff) == 911,
				      "One-shot does not match its saved monitor surface");
				for (auto *surface : { &side.tmap_num, &side.tmap_num2 }) {
					const int orientation = surface == &side.tmap_num2 ? static_cast<unsigned short>(*surface) & 0xc000 : 0;
					const int value = surface == &side.tmap_num2 ? static_cast<unsigned short>(*surface) & 0x3fff : *surface;
					const int target = value == 911 && surface == &side.tmap_num2 && monitor_surfaces.count({ i, side_index }) ? 386 : texture_target(value);
					check(target >= 0 && target < NumTextures, "Saved surface requires missing mod textures");
					if (value != target) {
						check(!exact_only, "Exact restore requires original textures");
						textures.emplace_back(surface, short(target | orientation));
					}
				}
			}
		std::vector<std::pair<int, fix>> clocks;
		check(!legacy_uud || Num_open_doors == 0, "Legacy save has moving doors; re-enable original mods and re-save");
		if (!legacy && changed)
			for (int i = 0; i < Num_open_doors; ++i) {
				auto &door = ActiveDoors[i];
				check(door.n_parts >= 1 && door.n_parts <= 2 && door.front_wallnum[0] >= 0 && door.front_wallnum[0] < Num_walls, "Invalid active door");
				const auto &wall = Walls[door.front_wallnum[0]];
				if (wall.state != WALL_DOOR_OPENING && wall.state != WALL_DOOR_CLOSING) continue;
				const int clip = wall.clip_num;
				check(clip >= 0 && clip < Num_wall_anims && clip < int(saved.at("walls").size()), "Active door requires missing mod animation");
				auto old = saved.at("walls").at(clip), now = wall_definition(WallAnims[clip]);
				const int old_time = old.at(0), new_time = now.at(0);
				old[0] = new_time;
				check(old == now, "Active door geometry changed");
				for (int part = 0; part < door.n_parts; ++part) {
					for (int face = 0; face < 2; ++face) {
						const int number = face ? door.back_wallnum[part] : door.front_wallnum[part];
						if (face && number == -1) continue;
						check(number >= 0 && number < Num_walls, "Invalid linked door wall");
						const int part_clip = Walls[number].clip_num;
						check(part_clip >= 0 && part_clip < Num_wall_anims && part_clip < int(saved.at("walls").size()), "Missing linked door animation");
						auto part_old = saved.at("walls").at(part_clip), part_now = wall_definition(WallAnims[part_clip]);
						const int part_old_time = part_old.at(0), part_new_time = part_now.at(0);
						part_old[0] = part_new_time;
						check(part_old == part_now && int64_t(part_old_time) * new_time == int64_t(part_new_time) * old_time, "Linked door animations changed inconsistently");
					}
				}
				if (old_time != new_time) {
					check(old_time > 0 && new_time > 0 && door.time >= 0, "Invalid active door duration");
					clocks.emplace_back(i, fix((std::min) (int64_t(door.time) * new_time / old_time, int64_t(new_time))));
				}
			}
		for (const auto &entry : textures) *entry.first = entry.second;
		for (const auto &entry : clocks) ActiveDoors[entry.first].time = entry.second;
		adapted += int(textures.size() + clocks.size());
		world_adapted = true;
		return 1;
	} catch (const std::exception &error) {
		return reject(error.what());
	}
}
void mod_save_begin(void)
{
	status = "none";
	reason.clear();
	error_pending = false;
	adapted = 0;
	changed = false;
}
void mod_save_finished(int success)
{
	error_pending = !success && status == "incompatible";
	if (!success) return;
	status = adapted || changed ? "presentation_adapted" : "exact";
	debug_log(DLOG_GAME, "Mod save restore: %s adapted=%d legacy=%d", status.c_str(), adapted, legacy);
	if (adapted || changed) HUD_init_message_literal(HM_DEFAULT, "Loaded with updated mod animations");
}
void mod_save_show_error(void)
{
	// Wait until failed-restore cleanup has created a complete menu
	if (!error_pending || Game_wind) return;
	error_pending = false;
	std::string message = reason;
	for (size_t line = 0; line + 48 < message.size();) {
		const auto space = message.rfind(' ', line + 48);
		if (space == std::string::npos || space < line) break;
		message[space] = '\n';
		line = space + 1;
	}
	char title[] = "Cannot load save";
	nm_messagebox(title, 1, "OK", "%s", message.c_str());
}
const char *mod_save_status(void)
{
	return status.c_str();
}
const char *mod_save_reason(void)
{
	return reason.c_str();
}
int mod_save_adapted_count(void)
{
	return adapted;
}
