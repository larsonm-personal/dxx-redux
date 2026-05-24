#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

extern "C" {
#include "pstypes.h"
#include "gr.h"
#include "bm.h"
#include "piggy.h"
#include "sounds.h"
#include "textures.h"
#include "vclip.h"
#include "effects.h"
#include "wall.h"
#include "robot.h"
#include "weapon.h"
#include "console.h"
#include "physfsx.h"
#include "dxa_metadata_patch.h"
}

using json = nlohmann::json;

namespace {
const char kD2HamPatchPath[] = "patches/d2/ham_patch.rfc6902.json";
const PHYSFS_sint64 kMaxPatchBytes = 2 * 1024 * 1024;
int g_max_virtual_bitmap_index = -1;

int required_int_value(const json &value, const char *description, int min_value, int max_value)
{
	if (!value.is_number_integer())
		throw std::runtime_error(std::string("missing integer value ") + description);
	long long result = value.get<long long>();
	if (result < min_value || result > max_value)
		throw std::runtime_error(std::string("integer value out of range ") + description);
	return static_cast<int>(result);
}

int required_int(const json &value, const char *key, int min_value, int max_value)
{
	auto it = value.find(key);
	if (it == value.end())
		throw std::runtime_error(std::string("missing integer field ") + key);
	return required_int_value(*it, key, min_value, max_value);
}

void note_bitmap_index(int bitmap_index)
{
	if (bitmap_index > g_max_virtual_bitmap_index)
		g_max_virtual_bitmap_index = bitmap_index;
}

void set_bitmap_index(bitmap_index *out, int value)
{
	if (value < 0 || value >= MAX_BITMAP_FILES)
		throw std::runtime_error("bitmap index out of range");
	out->index = static_cast<ushort>(value);
	note_bitmap_index(value);
}

void read_bitmap_frames(bitmap_index *frames, int count, const json &value)
{
	if (!value.is_array())
		throw std::runtime_error("frames field is not an array");
	for (int i = 0; i < count; i++) {
		int frame = 0;
		if (i < static_cast<int>(value.size())) {
			if (!value[i].is_number_integer())
				throw std::runtime_error("frame value is not an integer");
			frame = value[i].get<int>();
		}
		set_bitmap_index(&frames[i], frame);
	}
}

void read_wclip_frames(wclip &wall, const json &value)
{
	if (!value.is_array())
		throw std::runtime_error("frames field is not an array");
	for (int i = 0; i < MAX_CLIP_FRAMES; i++) {
		int frame = 0;
		if (i < static_cast<int>(value.size())) {
			if (!value[i].is_number_integer())
				throw std::runtime_error("frame value is not an integer");
			frame = value[i].get<int>();
		}
		if (frame < 0 || frame >= MAX_TEXTURES)
			throw std::runtime_error("wall frame out of range");
		wall.frames[i] = static_cast<short>(frame);
	}
}

bool read_physfs_text(const char *path, std::string &text)
{
	PHYSFS_file *file = PHYSFS_openRead(path);
	if (!file)
		return false;
	try {
		PHYSFS_sint64 length = PHYSFS_fileLength(file);
		if (length < 0 || length > kMaxPatchBytes)
			throw std::runtime_error("patch file size is out of range");
		text.assign(static_cast<size_t>(length), '\0');
		if (length && PHYSFS_readBytes(file, &text[0], length) != length)
			throw std::runtime_error("could not read patch file");
		PHYSFS_close(file);
		return true;
	} catch (...) {
		PHYSFS_close(file);
		throw;
	}
}

struct patch_path {
	std::string section;
	int index;
	std::string field;
};

patch_path parse_patch_path(const std::string &path)
{
	const std::string prefix = "/sections/";
	if (path.compare(0, prefix.size(), prefix) != 0)
		throw std::runtime_error("unsupported patch path");
	size_t slash = path.find('/', prefix.size());
	if (slash == std::string::npos)
		throw std::runtime_error("patch path is missing index");
	patch_path parsed;
	parsed.section = path.substr(prefix.size(), slash - prefix.size());
	size_t field_slash = path.find('/', slash + 1);
	std::string index_text = path.substr(slash + 1, field_slash == std::string::npos ? std::string::npos : field_slash - slash - 1);
	if (index_text.empty() || index_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("patch path index is invalid");
	parsed.index = std::stoi(index_text);
	if (field_slash != std::string::npos) {
		parsed.field = path.substr(field_slash + 1);
		if (parsed.field.empty() || parsed.field.find('/') != std::string::npos)
			throw std::runtime_error("patch path field is invalid");
	}
	return parsed;
}

std::string patch_path_text(const patch_path &path)
{
	std::string text = "/sections/" + path.section + "/" + std::to_string(path.index);
	if (!path.field.empty())
		text += "/" + path.field;
	return text;
}

std::string short_json_text(const json &value)
{
	std::string text = value.dump();
	if (text.size() > 240)
		text = text.substr(0, 240) + "...";
	return text;
}

json bitmap_frames_json(const bitmap_index *frames, int count)
{
	json result = json::array();
	for (int i = 0; i < count; i++)
		result.push_back(frames[i].index);
	return result;
}

json wclip_frames_json(const wclip &wall)
{
	json result = json::array();
	for (int i = 0; i < MAX_CLIP_FRAMES; i++)
		result.push_back(wall.frames[i]);
	return result;
}

template <typename T>
T packed_value_copy(T value)
{
	return value;
}

std::string fixed_string(const char *text, size_t max_size)
{
	size_t length = 0;
	while (length < max_size && text[length] != '\0')
		length++;
	return std::string(text, length);
}

json current_texture_value(int index)
{
	if (index < 0 || index >= NumTextures)
		throw std::runtime_error("texture test path is outside the current HAM texture table");
	return json{
		{"Index", index},
		{"Bitmap", packed_value_copy(Textures[index].index)},
		{"Flags", packed_value_copy(TmapInfo[index].flags)},
		{"Pad0", packed_value_copy(TmapInfo[index].pad[0])},
		{"Pad1", packed_value_copy(TmapInfo[index].pad[1])},
		{"Pad2", packed_value_copy(TmapInfo[index].pad[2])},
		{"Lighting", packed_value_copy(TmapInfo[index].lighting)},
		{"Damage", packed_value_copy(TmapInfo[index].damage)},
		{"Eclip", packed_value_copy(TmapInfo[index].eclip_num)},
		{"Destroyed", packed_value_copy(TmapInfo[index].destroyed)},
		{"SlideU", packed_value_copy(TmapInfo[index].slide_u)},
		{"SlideV", packed_value_copy(TmapInfo[index].slide_v)},
	};
}

json current_vclip_value(int index)
{
	if (index < 0 || index >= Num_vclips)
		throw std::runtime_error("vclip test path is outside the current HAM vclip table");
	vclip &clip = Vclip[index];
	return json{
		{"Index", index},
		{"PlayTime", packed_value_copy(clip.play_time)},
		{"NumFrames", packed_value_copy(clip.num_frames)},
		{"FrameTime", packed_value_copy(clip.frame_time)},
		{"Flags", packed_value_copy(clip.flags)},
		{"Sound", packed_value_copy(clip.sound_num)},
		{"Frames", bitmap_frames_json(clip.frames, VCLIP_MAX_FRAMES)},
		{"Light", packed_value_copy(clip.light_value)},
	};
}

json current_eclip_value(int index)
{
	if (index < 0 || index >= Num_effects)
		throw std::runtime_error("eclip test path is outside the current HAM eclip table");
	eclip &effect = Effects[index];
	return json{
		{"Index", index},
		{"PlayTime", packed_value_copy(effect.vc.play_time)},
		{"NumFrames", packed_value_copy(effect.vc.num_frames)},
		{"FrameTime", packed_value_copy(effect.vc.frame_time)},
		{"VclipFlags", packed_value_copy(effect.vc.flags)},
		{"VclipSound", packed_value_copy(effect.vc.sound_num)},
		{"Frames", bitmap_frames_json(effect.vc.frames, VCLIP_MAX_FRAMES)},
		{"Light", packed_value_copy(effect.vc.light_value)},
		{"TimeLeft", packed_value_copy(effect.time_left)},
		{"FrameCount", packed_value_copy(effect.frame_count)},
		{"ChangingWall", packed_value_copy(effect.changing_wall_texture)},
		{"ChangingObject", packed_value_copy(effect.changing_object_texture)},
		{"Flags", packed_value_copy(effect.flags)},
		{"CritClip", packed_value_copy(effect.crit_clip)},
		{"DestBm", packed_value_copy(effect.dest_bm_num)},
		{"DestVclip", packed_value_copy(effect.dest_vclip)},
		{"DestEclip", packed_value_copy(effect.dest_eclip)},
		{"DestSize", packed_value_copy(effect.dest_size)},
		{"Sound", packed_value_copy(effect.sound_num)},
		{"Seg", packed_value_copy(effect.segnum)},
		{"Side", packed_value_copy(effect.sidenum)},
	};
}

json current_wclip_value(int index)
{
	if (index < 0 || index >= Num_wall_anims)
		throw std::runtime_error("wclip test path is outside the current HAM wall animation table");
	wclip &wall = WallAnims[index];
	return json{
		{"Index", index},
		{"PlayTime", packed_value_copy(wall.play_time)},
		{"NumFrames", packed_value_copy(wall.num_frames)},
		{"Frames", wclip_frames_json(wall)},
		{"OpenSound", packed_value_copy(wall.open_sound)},
		{"CloseSound", packed_value_copy(wall.close_sound)},
		{"Flags", packed_value_copy(wall.flags)},
		{"Filename", fixed_string(wall.filename, sizeof(wall.filename))},
		{"Pad", packed_value_copy(wall.pad)},
	};
}

json current_sound_value(int index)
{
	if (index < 0 || index >= MAX_SOUNDS)
		throw std::runtime_error("sound test path is outside the current HAM sound table");
	return json{
		{"Index", index},
		{"Sound", static_cast<int>(Sounds[index])},
		{"AltSound", static_cast<int>(AltSounds[index])},
	};
}

json current_obj_bitmap_value(int index)
{
	if (index < 0 || index >= N_ObjBitmaps)
		throw std::runtime_error("object bitmap test path is outside the current HAM object bitmap table");
	return json{
		{"Index", index},
		{"Bitmap", packed_value_copy(ObjBitmaps[index].index)},
		{"Pointer", ObjBitmapPtrs[index]},
	};
}

bool parse_indexed_field(const std::string &field, const char *prefix, int max_count, int &index)
{
	std::string prefix_text(prefix);
	if (field.compare(0, prefix_text.size(), prefix_text) != 0)
		return false;
	std::string index_text = field.substr(prefix_text.size());
	if (index_text.empty() || index_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("invalid indexed robot HAM patch field");
	index = std::stoi(index_text);
	if (index < 0 || index >= max_count)
		throw std::runtime_error("indexed robot HAM patch field out of range");
	return true;
}

bool parse_gun_point_field(const std::string &field, int &gun, char &axis)
{
	const std::string prefix = "GunPoint";
	if (field.compare(0, prefix.size(), prefix) != 0)
		return false;
	if (field.size() <= prefix.size() + 1)
		throw std::runtime_error("invalid gun point HAM patch field");
	axis = field[field.size() - 1];
	if (axis != 'X' && axis != 'Y' && axis != 'Z')
		throw std::runtime_error("invalid gun point HAM patch axis");
	std::string index_text = field.substr(prefix.size(), field.size() - prefix.size() - 1);
	if (index_text.empty() || index_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("invalid gun point HAM patch index");
	gun = std::stoi(index_text);
	if (gun < 0 || gun >= MAX_GUNS)
		throw std::runtime_error("gun point HAM patch index out of range");
	return true;
}

bool parse_anim_state_field(const std::string &field, int &gun, int &state, bool &offset_field)
{
	const std::string prefix = "AnimState";
	if (field.compare(0, prefix.size(), prefix) != 0)
		return false;
	size_t underscore = field.find('_', prefix.size());
	if (underscore == std::string::npos)
		throw std::runtime_error("invalid animation state HAM patch field");
	std::string gun_text = field.substr(prefix.size(), underscore - prefix.size());
	if (gun_text.empty() || gun_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("invalid animation state gun index");
	gun = std::stoi(gun_text);
	if (gun < 0 || gun >= MAX_GUNS + 1)
		throw std::runtime_error("animation state gun index out of range");
	std::string rest = field.substr(underscore + 1);
	const std::string joints_suffix = "Joints";
	const std::string offset_suffix = "Offset";
	std::string state_text;
	if (rest.size() > joints_suffix.size() && rest.compare(rest.size() - joints_suffix.size(), joints_suffix.size(), joints_suffix) == 0) {
		state_text = rest.substr(0, rest.size() - joints_suffix.size());
		offset_field = false;
	} else if (rest.size() > offset_suffix.size() && rest.compare(rest.size() - offset_suffix.size(), offset_suffix.size(), offset_suffix) == 0) {
		state_text = rest.substr(0, rest.size() - offset_suffix.size());
		offset_field = true;
	} else
		throw std::runtime_error("invalid animation state field suffix");
	if (state_text.empty() || state_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("invalid animation state index");
	state = std::stoi(state_text);
	if (state < 0 || state >= N_ANIM_STATES)
		throw std::runtime_error("animation state index out of range");
	return true;
}

fix robot_gun_point_value(const robot_info &robot, int gun, char axis)
{
	if (axis == 'X')
		return robot.gun_points[gun].x;
	if (axis == 'Y')
		return robot.gun_points[gun].y;
	return robot.gun_points[gun].z;
}

void set_robot_gun_point_value(robot_info &robot, int gun, char axis, fix value)
{
	if (axis == 'X')
		robot.gun_points[gun].x = value;
	else if (axis == 'Y')
		robot.gun_points[gun].y = value;
	else
		robot.gun_points[gun].z = value;
}

bool read_robot_fix_array_field(const robot_info &robot, const std::string &field,
	fix &value)
{
	int index = 0;
	if (parse_indexed_field(field, "FieldOfView", NDL, index))
		value = robot.field_of_view[index];
	else if (parse_indexed_field(field, "FiringWait2", NDL, index))
		value = robot.firing_wait2[index];
	else if (parse_indexed_field(field, "FiringWait", NDL, index))
		value = robot.firing_wait[index];
	else if (parse_indexed_field(field, "TurnTime", NDL, index))
		value = robot.turn_time[index];
	else if (parse_indexed_field(field, "MaxSpeed", NDL, index))
		value = robot.max_speed[index];
	else if (parse_indexed_field(field, "CircleDistance", NDL, index))
		value = robot.circle_distance[index];
	else
		return false;
	return true;
}

bool apply_robot_fix_array_field(robot_info &robot, const std::string &field,
	const json &value)
{
	int index = 0;
	fix field_value = required_int_value(value, field.c_str(), -0x40000000,
		0x40000000);
	if (parse_indexed_field(field, "FieldOfView", NDL, index))
		robot.field_of_view[index] = field_value;
	else if (parse_indexed_field(field, "FiringWait2", NDL, index))
		robot.firing_wait2[index] = field_value;
	else if (parse_indexed_field(field, "FiringWait", NDL, index))
		robot.firing_wait[index] = field_value;
	else if (parse_indexed_field(field, "TurnTime", NDL, index))
		robot.turn_time[index] = field_value;
	else if (parse_indexed_field(field, "MaxSpeed", NDL, index))
		robot.max_speed[index] = field_value;
	else if (parse_indexed_field(field, "CircleDistance", NDL, index))
		robot.circle_distance[index] = field_value;
	else
		return false;
	return true;
}

json current_robot_field_value(int index, const std::string &field)
{
	if (index < 0 || index >= N_robot_types)
		throw std::runtime_error("robot test path is outside the current HAM robot table");
	robot_info &robot = Robot_info[index];
	int field_index = 0;
	int state_index = 0;
	char axis = 'X';
	bool offset_field = false;
	fix fix_field_value = 0;
	if (parse_gun_point_field(field, field_index, axis))
		return robot_gun_point_value(robot, field_index, axis);
	if (parse_anim_state_field(field, field_index, state_index, offset_field))
		return packed_value_copy(offset_field ? robot.anim_states[field_index][state_index].offset : robot.anim_states[field_index][state_index].n_joints);
	if (parse_indexed_field(field, "GunSubmodel", MAX_GUNS, field_index))
		return static_cast<int>(robot.gun_submodels[field_index]);
	if (read_robot_fix_array_field(robot, field, fix_field_value))
		return fix_field_value;
	if (parse_indexed_field(field, "RapidfireCount", NDL, field_index))
		return static_cast<int>(static_cast<ubyte>(robot.rapidfire_count[field_index]));
	if (parse_indexed_field(field, "EvadeSpeed", NDL, field_index))
		return static_cast<int>(static_cast<ubyte>(robot.evade_speed[field_index]));
	if (field == "Exp1Sound")
		return packed_value_copy(robot.exp1_sound_num);
	if (field == "Exp1Vclip")
		return packed_value_copy(robot.exp1_vclip_num);
	if (field == "Exp2Sound")
		return packed_value_copy(robot.exp2_sound_num);
	if (field == "Exp2Vclip")
		return packed_value_copy(robot.exp2_vclip_num);
	if (field == "WeaponType")
		return static_cast<int>(static_cast<ubyte>(robot.weapon_type));
	if (field == "WeaponType2")
		return static_cast<int>(static_cast<ubyte>(robot.weapon_type2));
	if (field == "NGuns")
		return static_cast<int>(static_cast<ubyte>(robot.n_guns));
	if (field == "ContainsId")
		return static_cast<int>(static_cast<ubyte>(robot.contains_id));
	if (field == "ContainsCount")
		return static_cast<int>(static_cast<ubyte>(robot.contains_count));
	if (field == "ContainsProb")
		return static_cast<int>(static_cast<ubyte>(robot.contains_prob));
	if (field == "ContainsType")
		return static_cast<int>(static_cast<ubyte>(robot.contains_type));
	if (field == "Kamikaze")
		return static_cast<int>(static_cast<ubyte>(robot.kamikaze));
	if (field == "ScoreValue")
		return packed_value_copy(robot.score_value);
	if (field == "Badass")
		return static_cast<int>(static_cast<ubyte>(robot.badass));
	if (field == "EnergyDrain")
		return static_cast<int>(static_cast<ubyte>(robot.energy_drain));
	if (field == "Lighting")
		return packed_value_copy(robot.lighting);
	if (field == "Strength")
		return packed_value_copy(robot.strength);
	if (field == "Mass")
		return packed_value_copy(robot.mass);
	if (field == "Drag")
		return packed_value_copy(robot.drag);
	if (field == "CloakType")
		return static_cast<int>(static_cast<ubyte>(robot.cloak_type));
	if (field == "AttackType")
		return static_cast<int>(static_cast<ubyte>(robot.attack_type));
	if (field == "SeeSound")
		return static_cast<int>(robot.see_sound);
	if (field == "AttackSound")
		return static_cast<int>(robot.attack_sound);
	if (field == "ClawSound")
		return static_cast<int>(robot.claw_sound);
	if (field == "TauntSound")
		return static_cast<int>(robot.taunt_sound);
	if (field == "BossFlag")
		return static_cast<int>(static_cast<ubyte>(robot.boss_flag));
	if (field == "Companion")
		return static_cast<int>(static_cast<ubyte>(robot.companion));
	if (field == "SmartBlobs")
		return static_cast<int>(static_cast<ubyte>(robot.smart_blobs));
	if (field == "EnergyBlobs")
		return static_cast<int>(static_cast<ubyte>(robot.energy_blobs));
	if (field == "Thief")
		return static_cast<int>(static_cast<ubyte>(robot.thief));
	if (field == "Pursuit")
		return static_cast<int>(static_cast<ubyte>(robot.pursuit));
	if (field == "Lightcast")
		return static_cast<int>(static_cast<ubyte>(robot.lightcast));
	if (field == "DeathRoll")
		return static_cast<int>(static_cast<ubyte>(robot.death_roll));
	if (field == "Flags")
		return static_cast<int>(robot.flags);
	if (field == "DeathrollSound")
		return static_cast<int>(robot.deathroll_sound);
	if (field == "Glow")
		return static_cast<int>(robot.glow);
	if (field == "Behavior")
		return static_cast<int>(robot.behavior);
	if (field == "Aim")
		return static_cast<int>(robot.aim);
	if (field == "Always0xabcd")
		return packed_value_copy(robot.always_0xabcd);
	throw std::runtime_error("unsupported robot HAM patch field");
}

json current_weapon_field_value(int index, const std::string &field)
{
	if (index < 0 || index >= N_weapon_types)
		throw std::runtime_error("weapon test path is outside the current HAM weapon table");
	weapon_info &weapon = Weapon_info[index];
	if (field == "FlashSound")
		return packed_value_copy(weapon.flash_sound);
	if (field == "RobotHitSound")
		return packed_value_copy(weapon.robot_hit_sound);
	if (field == "WallHitSound")
		return packed_value_copy(weapon.wall_hit_sound);
	throw std::runtime_error("unsupported weapon HAM patch field");
}

json field_value(const json &object, const std::string &field)
{
	auto it = object.find(field);
	if (it == object.end())
		throw std::runtime_error("unsupported HAM patch field " + field);
	return *it;
}

json current_patch_row_value(const patch_path &path)
{
	if (path.section == "textures")
		return current_texture_value(path.index);
	if (path.section == "vclips")
		return current_vclip_value(path.index);
	if (path.section == "eclips")
		return current_eclip_value(path.index);
	if (path.section == "wclips")
		return current_wclip_value(path.index);
	if (path.section == "sounds")
		return current_sound_value(path.index);
	if (path.section == "objBitmaps")
		return current_obj_bitmap_value(path.index);
	throw std::runtime_error("unsupported HAM patch test section");
}

json current_patch_value(const patch_path &path)
{
	if (path.field.empty())
		return current_patch_row_value(path);
	if (path.section == "robots")
		return current_robot_field_value(path.index, path.field);
	if (path.section == "weapons")
		return current_weapon_field_value(path.index, path.field);
	return field_value(current_patch_row_value(path), path.field);
}

void validate_patch_test(size_t op_index, const patch_path &path, const json &expected)
{
	json actual = current_patch_value(path);
	if (actual != expected)
		throw std::runtime_error("HAM patch test failed at op " + std::to_string(op_index) + " " +
			patch_path_text(path) + "; expected " + short_json_text(expected) + "; actual " +
			short_json_text(actual));
}

void apply_texture(int index, const json &value)
{
	if (index < 0 || index >= MAX_TEXTURES)
		throw std::runtime_error("texture index out of range");
	int value_index = required_int(value, "Index", 0, MAX_TEXTURES - 1);
	if (value_index != index)
		throw std::runtime_error("texture index field does not match path");
	set_bitmap_index(&Textures[index], required_int(value, "Bitmap", 0, MAX_BITMAP_FILES - 1));
	TmapInfo[index].flags = static_cast<ubyte>(required_int(value, "Flags", 0, 255));
	TmapInfo[index].pad[0] = static_cast<ubyte>(required_int(value, "Pad0", 0, 255));
	TmapInfo[index].pad[1] = static_cast<ubyte>(required_int(value, "Pad1", 0, 255));
	TmapInfo[index].pad[2] = static_cast<ubyte>(required_int(value, "Pad2", 0, 255));
	TmapInfo[index].lighting = required_int(value, "Lighting", -0x40000000, 0x40000000);
	TmapInfo[index].damage = required_int(value, "Damage", -0x40000000, 0x40000000);
	TmapInfo[index].eclip_num = static_cast<short>(required_int(value, "Eclip", -1, MAX_EFFECTS - 1));
	TmapInfo[index].destroyed = static_cast<short>(required_int(value, "Destroyed", -1, MAX_TEXTURES - 1));
	TmapInfo[index].slide_u = static_cast<short>(required_int(value, "SlideU", -32768, 32767));
	TmapInfo[index].slide_v = static_cast<short>(required_int(value, "SlideV", -32768, 32767));
	NumTextures = (std::max)(NumTextures, index + 1);
}

void apply_vclip(int index, const json &value)
{
	if (index < 0 || index >= VCLIP_MAXNUM)
		throw std::runtime_error("vclip index out of range");
	int value_index = required_int(value, "Index", 0, VCLIP_MAXNUM - 1);
	if (value_index != index)
		throw std::runtime_error("vclip index field does not match path");
	vclip &clip = Vclip[index];
	clip.play_time = required_int(value, "PlayTime", -0x40000000, 0x40000000);
	clip.num_frames = required_int(value, "NumFrames", -1, VCLIP_MAX_FRAMES);
	clip.frame_time = required_int(value, "FrameTime", -0x40000000, 0x40000000);
	clip.flags = required_int(value, "Flags", -0x40000000, 0x40000000);
	clip.sound_num = static_cast<short>(required_int(value, "Sound", -1, MAX_SOUNDS - 1));
	read_bitmap_frames(clip.frames, VCLIP_MAX_FRAMES, value.at("Frames"));
	clip.light_value = required_int(value, "Light", -0x40000000, 0x40000000);
	Num_vclips = (std::max)(Num_vclips, index + 1);
}

void apply_eclip(int index, const json &value)
{
	if (index < 0 || index >= MAX_EFFECTS)
		throw std::runtime_error("eclip index out of range");
	int value_index = required_int(value, "Index", 0, MAX_EFFECTS - 1);
	if (value_index != index)
		throw std::runtime_error("eclip index field does not match path");
	eclip &effect = Effects[index];
	effect.vc.play_time = required_int(value, "PlayTime", -0x40000000, 0x40000000);
	effect.vc.num_frames = required_int(value, "NumFrames", -1, VCLIP_MAX_FRAMES);
	effect.vc.frame_time = required_int(value, "FrameTime", -0x40000000, 0x40000000);
	effect.vc.flags = required_int(value, "VclipFlags", -0x40000000, 0x40000000);
	effect.vc.sound_num = static_cast<short>(required_int(value, "VclipSound", -1, MAX_SOUNDS - 1));
	read_bitmap_frames(effect.vc.frames, VCLIP_MAX_FRAMES, value.at("Frames"));
	effect.vc.light_value = required_int(value, "Light", -0x40000000, 0x40000000);
	effect.time_left = required_int(value, "TimeLeft", -0x40000000, 0x40000000);
	effect.frame_count = required_int(value, "FrameCount", -0x40000000, 0x40000000);
	effect.changing_wall_texture = static_cast<short>(required_int(value, "ChangingWall", -1, MAX_TEXTURES - 1));
	effect.changing_object_texture = static_cast<short>(required_int(value, "ChangingObject", -1, MAX_OBJ_BITMAPS - 1));
	effect.flags = required_int(value, "Flags", -0x40000000, 0x40000000);
	effect.crit_clip = required_int(value, "CritClip", -1, MAX_EFFECTS - 1);
	effect.dest_bm_num = required_int(value, "DestBm", -1, MAX_TEXTURES - 1);
	effect.dest_vclip = required_int(value, "DestVclip", -1, VCLIP_MAXNUM - 1);
	effect.dest_eclip = required_int(value, "DestEclip", -1, MAX_EFFECTS - 1);
	effect.dest_size = required_int(value, "DestSize", -0x40000000, 0x40000000);
	effect.sound_num = required_int(value, "Sound", -1, MAX_SOUNDS - 1);
	effect.segnum = required_int(value, "Seg", -1, 32767);
	effect.sidenum = required_int(value, "Side", 0, 5);
	Num_effects = (std::max)(Num_effects, index + 1);
}

void apply_wclip(int index, const json &value)
{
	if (index < 0 || index >= MAX_WALL_ANIMS)
		throw std::runtime_error("wclip index out of range");
	int value_index = required_int(value, "Index", 0, MAX_WALL_ANIMS - 1);
	if (value_index != index)
		throw std::runtime_error("wclip index field does not match path");
	wclip &wall = WallAnims[index];
	wall.play_time = required_int(value, "PlayTime", -0x40000000, 0x40000000);
	wall.num_frames = static_cast<short>(required_int(value, "NumFrames", -1, MAX_CLIP_FRAMES));
	read_wclip_frames(wall, value.at("Frames"));
	wall.open_sound = static_cast<short>(required_int(value, "OpenSound", -1, MAX_SOUNDS - 1));
	wall.close_sound = static_cast<short>(required_int(value, "CloseSound", -1, MAX_SOUNDS - 1));
	wall.flags = static_cast<short>(required_int(value, "Flags", -32768, 32767));
	std::string filename = value.value("Filename", std::string());
	std::memset(wall.filename, 0, sizeof(wall.filename));
	std::memcpy(wall.filename, filename.data(), (std::min)(filename.size(), sizeof(wall.filename)));
	wall.pad = static_cast<char>(required_int(value, "Pad", -128, 127));
	Num_wall_anims = (std::max)(Num_wall_anims, index + 1);
}

void apply_sound_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= MAX_SOUNDS)
		throw std::runtime_error("sound index out of range");
	ubyte sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	if (field == "Sound")
		Sounds[index] = sound;
	else if (field == "AltSound")
		AltSounds[index] = sound;
	else
		throw std::runtime_error("unsupported sound HAM patch field");
}

void apply_eclip_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= Num_effects)
		throw std::runtime_error("eclip index out of range");
	eclip &effect = Effects[index];
	if (field == "FrameTime")
		effect.vc.frame_time = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "Sound")
		effect.sound_num = required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1);
	else
		throw std::runtime_error("unsupported eclip HAM patch field");
}

void apply_wclip_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= Num_wall_anims)
		throw std::runtime_error("wclip index out of range");
	wclip &wall = WallAnims[index];
	if (field == "PlayTime")
		wall.play_time = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "OpenSound")
		wall.open_sound = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else if (field == "CloseSound")
		wall.close_sound = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else
		throw std::runtime_error("unsupported wclip HAM patch field");
}

void apply_robot_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= N_robot_types)
		throw std::runtime_error("robot index out of range");
	robot_info &robot = Robot_info[index];
	int field_index = 0;
	int state_index = 0;
	char axis = 'X';
	bool offset_field = false;
	if (parse_gun_point_field(field, field_index, axis))
		set_robot_gun_point_value(robot, field_index, axis,
			required_int_value(value, field.c_str(), -0x40000000, 0x40000000));
	else if (parse_anim_state_field(field, field_index, state_index, offset_field)) {
		short field_value = static_cast<short>(required_int_value(value, field.c_str(), -32768, 32767));
		if (offset_field)
			robot.anim_states[field_index][state_index].offset = field_value;
		else
			robot.anim_states[field_index][state_index].n_joints = field_value;
	}
	else if (parse_indexed_field(field, "GunSubmodel", MAX_GUNS, field_index))
		robot.gun_submodels[field_index] = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (apply_robot_fix_array_field(robot, field, value))
		;
	else if (parse_indexed_field(field, "RapidfireCount", NDL, field_index))
		robot.rapidfire_count[field_index] = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (parse_indexed_field(field, "EvadeSpeed", NDL, field_index))
		robot.evade_speed[field_index] = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Exp1Sound")
		robot.exp1_sound_num = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else if (field == "Exp1Vclip")
		robot.exp1_vclip_num = static_cast<short>(required_int_value(value, field.c_str(), -1, VCLIP_MAXNUM - 1));
	else if (field == "Exp2Sound")
		robot.exp2_sound_num = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else if (field == "Exp2Vclip")
		robot.exp2_vclip_num = static_cast<short>(required_int_value(value, field.c_str(), -1, VCLIP_MAXNUM - 1));
	else if (field == "WeaponType")
		robot.weapon_type = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "WeaponType2")
		robot.weapon_type2 = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "NGuns")
		robot.n_guns = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, MAX_GUNS));
	else if (field == "ContainsId")
		robot.contains_id = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "ContainsCount")
		robot.contains_count = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "ContainsProb")
		robot.contains_prob = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "ContainsType")
		robot.contains_type = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Kamikaze")
		robot.kamikaze = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "ScoreValue")
		robot.score_value = static_cast<short>(required_int_value(value, field.c_str(), -32768, 32767));
	else if (field == "Badass")
		robot.badass = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "EnergyDrain")
		robot.energy_drain = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Lighting")
		robot.lighting = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "Strength")
		robot.strength = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "Mass")
		robot.mass = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "Drag")
		robot.drag = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else if (field == "CloakType")
		robot.cloak_type = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "AttackType")
		robot.attack_type = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "SeeSound")
		robot.see_sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "AttackSound")
		robot.attack_sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "ClawSound")
		robot.claw_sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "TauntSound")
		robot.taunt_sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "BossFlag")
		robot.boss_flag = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Companion")
		robot.companion = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "SmartBlobs")
		robot.smart_blobs = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "EnergyBlobs")
		robot.energy_blobs = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Thief")
		robot.thief = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Pursuit")
		robot.pursuit = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Lightcast")
		robot.lightcast = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "DeathRoll")
		robot.death_roll = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Flags")
		robot.flags = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "DeathrollSound")
		robot.deathroll_sound = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Glow")
		robot.glow = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Behavior")
		robot.behavior = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Aim")
		robot.aim = static_cast<ubyte>(required_int_value(value, field.c_str(), 0, 255));
	else if (field == "Always0xabcd")
		robot.always_0xabcd = required_int_value(value, field.c_str(), -0x40000000, 0x40000000);
	else
		throw std::runtime_error("unsupported robot HAM patch field");
}

void apply_weapon_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= N_weapon_types)
		throw std::runtime_error("weapon index out of range");
	weapon_info &weapon = Weapon_info[index];
	if (field == "FlashSound")
		weapon.flash_sound = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else if (field == "RobotHitSound")
		weapon.robot_hit_sound = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else if (field == "WallHitSound")
		weapon.wall_hit_sound = static_cast<short>(required_int_value(value, field.c_str(), -1, MAX_SOUNDS - 1));
	else
		throw std::runtime_error("unsupported weapon HAM patch field");
}

void apply_obj_bitmap_field(int index, const std::string &field, const json &value)
{
	if (index < 0 || index >= N_ObjBitmaps)
		throw std::runtime_error("object bitmap index out of range");
	if (field == "Bitmap")
		set_bitmap_index(&ObjBitmaps[index], required_int_value(value, field.c_str(), 0, MAX_BITMAP_FILES - 1));
	else if (field == "Pointer")
		ObjBitmapPtrs[index] = static_cast<ushort>(required_int_value(value, field.c_str(), 0, MAX_OBJ_BITMAPS - 1));
	else
		throw std::runtime_error("unsupported object bitmap HAM patch field");
}

void apply_patch_field(const patch_path &path, const json &value)
{
	if (path.section == "sounds")
		apply_sound_field(path.index, path.field, value);
	else if (path.section == "eclips")
		apply_eclip_field(path.index, path.field, value);
	else if (path.section == "wclips")
		apply_wclip_field(path.index, path.field, value);
	else if (path.section == "robots")
		apply_robot_field(path.index, path.field, value);
	else if (path.section == "weapons")
		apply_weapon_field(path.index, path.field, value);
	else if (path.section == "objBitmaps")
		apply_obj_bitmap_field(path.index, path.field, value);
	else
		throw std::runtime_error("unsupported HAM patch field section");
}

void apply_patch_value(const patch_path &path, const json &value)
{
	if (!path.field.empty())
		apply_patch_field(path, value);
	else if (path.section == "textures")
		apply_texture(path.index, value);
	else if (path.section == "vclips")
		apply_vclip(path.index, value);
	else if (path.section == "eclips")
		apply_eclip(path.index, value);
	else if (path.section == "wclips")
		apply_wclip(path.index, value);
	else
		throw std::runtime_error("unsupported HAM patch section");
}
} // namespace

extern "C" void dxa_metadata_patch_apply_d2_ham(void)
{
	std::string text;
	try {
		if (!read_physfs_text(kD2HamPatchPath, text))
			return;
		json patch = json::parse(text);
		if (!patch.is_array())
			throw std::runtime_error("HAM patch root is not an array");
		int tested = 0;
		for (size_t op_index = 0; op_index < patch.size(); op_index++) {
			const json &op = patch[op_index];
			std::string operation = op.value("op", std::string());
			if (operation == "test") {
				patch_path path = parse_patch_path(op.value("path", std::string()));
				auto value = op.find("value");
				if (value == op.end())
					throw std::runtime_error("HAM patch test operation is missing value");
				validate_patch_test(op_index, path, *value);
				tested++;
			} else if (operation != "add" && operation != "replace")
				throw std::runtime_error("unsupported HAM patch operation");
		}
		int applied = 0;
		for (const json &op : patch) {
			std::string operation = op.value("op", std::string());
			if (operation == "test")
				continue;
			if (operation != "add" && operation != "replace")
				throw std::runtime_error("unsupported HAM patch operation");
			patch_path path = parse_patch_path(op.value("path", std::string()));
			auto value = op.find("value");
			if (value == op.end())
				throw std::runtime_error("HAM patch operation is missing value");
			apply_patch_value(path, *value);
			applied++;
		}
		con_printf(CON_NORMAL, "DXA metadata: applied %d HAM patch operations after %d tests\n", applied, tested);
	} catch (const std::exception &ex) {
		con_printf(CON_URGENT, "DXA metadata: failed to apply %s: %s\n", kD2HamPatchPath, ex.what());
	}
}

extern "C" void dxa_metadata_patch_register_virtual_bitmaps(void)
{
	for (int index = Num_bitmap_files; index <= g_max_virtual_bitmap_index; index++)
		piggy_register_virtual_bitmap_index(index);
}
