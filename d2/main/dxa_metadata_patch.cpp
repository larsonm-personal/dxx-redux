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
#include "textures.h"
#include "vclip.h"
#include "effects.h"
#include "wall.h"
#include "console.h"
#include "physfsx.h"
#include "dxa_metadata_patch.h"
}

using json = nlohmann::json;

namespace {
const char kD2HamPatchPath[] = "patches/d2/ham_patch.rfc6902.json";
const PHYSFS_sint64 kMaxPatchBytes = 2 * 1024 * 1024;
int g_max_virtual_bitmap_index = -1;

int required_int(const json &value, const char *key, int min_value, int max_value)
{
	auto it = value.find(key);
	if (it == value.end() || !it->is_number_integer())
		throw std::runtime_error(std::string("missing integer field ") + key);
	long long result = it->get<long long>();
	if (result < min_value || result > max_value)
		throw std::runtime_error(std::string("integer field out of range ") + key);
	return static_cast<int>(result);
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

void read_short_frames(short *frames, int count, const json &value)
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
		if (frame < 0 || frame >= MAX_TEXTURES)
			throw std::runtime_error("wall frame out of range");
		frames[i] = static_cast<short>(frame);
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
	std::string index_text = path.substr(slash + 1);
	if (index_text.empty() || index_text.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("patch path index is invalid");
	parsed.index = std::stoi(index_text);
	return parsed;
}

std::string patch_path_text(const patch_path &path)
{
	return "/sections/" + path.section + "/" + std::to_string(path.index);
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

json short_frames_json(const short *frames, int count)
{
	json result = json::array();
	for (int i = 0; i < count; i++)
		result.push_back(frames[i]);
	return result;
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
		{"Bitmap", Textures[index].index},
		{"Flags", TmapInfo[index].flags},
		{"Pad0", TmapInfo[index].pad[0]},
		{"Pad1", TmapInfo[index].pad[1]},
		{"Pad2", TmapInfo[index].pad[2]},
		{"Lighting", TmapInfo[index].lighting},
		{"Damage", TmapInfo[index].damage},
		{"Eclip", TmapInfo[index].eclip_num},
		{"Destroyed", TmapInfo[index].destroyed},
		{"SlideU", TmapInfo[index].slide_u},
		{"SlideV", TmapInfo[index].slide_v},
	};
}

json current_vclip_value(int index)
{
	if (index < 0 || index >= Num_vclips)
		throw std::runtime_error("vclip test path is outside the current HAM vclip table");
	vclip &clip = Vclip[index];
	return json{
		{"Index", index},
		{"PlayTime", clip.play_time},
		{"NumFrames", clip.num_frames},
		{"FrameTime", clip.frame_time},
		{"Flags", clip.flags},
		{"Sound", clip.sound_num},
		{"Frames", bitmap_frames_json(clip.frames, VCLIP_MAX_FRAMES)},
		{"Light", clip.light_value},
	};
}

json current_eclip_value(int index)
{
	if (index < 0 || index >= Num_effects)
		throw std::runtime_error("eclip test path is outside the current HAM eclip table");
	eclip &effect = Effects[index];
	return json{
		{"Index", index},
		{"PlayTime", effect.vc.play_time},
		{"NumFrames", effect.vc.num_frames},
		{"FrameTime", effect.vc.frame_time},
		{"VclipFlags", effect.vc.flags},
		{"VclipSound", effect.vc.sound_num},
		{"Frames", bitmap_frames_json(effect.vc.frames, VCLIP_MAX_FRAMES)},
		{"Light", effect.vc.light_value},
		{"TimeLeft", effect.time_left},
		{"FrameCount", effect.frame_count},
		{"ChangingWall", effect.changing_wall_texture},
		{"ChangingObject", effect.changing_object_texture},
		{"Flags", effect.flags},
		{"CritClip", effect.crit_clip},
		{"DestBm", effect.dest_bm_num},
		{"DestVclip", effect.dest_vclip},
		{"DestEclip", effect.dest_eclip},
		{"DestSize", effect.dest_size},
		{"Sound", effect.sound_num},
		{"Seg", effect.segnum},
		{"Side", effect.sidenum},
	};
}

json current_wclip_value(int index)
{
	if (index < 0 || index >= Num_wall_anims)
		throw std::runtime_error("wclip test path is outside the current HAM wall animation table");
	wclip &wall = WallAnims[index];
	return json{
		{"Index", index},
		{"PlayTime", wall.play_time},
		{"NumFrames", wall.num_frames},
		{"Frames", short_frames_json(wall.frames, MAX_CLIP_FRAMES)},
		{"OpenSound", wall.open_sound},
		{"CloseSound", wall.close_sound},
		{"Flags", wall.flags},
		{"Filename", fixed_string(wall.filename, sizeof(wall.filename))},
		{"Pad", wall.pad},
	};
}

json current_patch_value(const patch_path &path)
{
	if (path.section == "textures")
		return current_texture_value(path.index);
	if (path.section == "vclips")
		return current_vclip_value(path.index);
	if (path.section == "eclips")
		return current_eclip_value(path.index);
	if (path.section == "wclips")
		return current_wclip_value(path.index);
	throw std::runtime_error("unsupported HAM patch test section");
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
	read_short_frames(wall.frames, MAX_CLIP_FRAMES, value.at("Frames"));
	wall.open_sound = static_cast<short>(required_int(value, "OpenSound", -1, MAX_SOUNDS - 1));
	wall.close_sound = static_cast<short>(required_int(value, "CloseSound", -1, MAX_SOUNDS - 1));
	wall.flags = static_cast<short>(required_int(value, "Flags", -32768, 32767));
	std::string filename = value.value("Filename", std::string());
	std::memset(wall.filename, 0, sizeof(wall.filename));
	std::memcpy(wall.filename, filename.data(), (std::min)(filename.size(), sizeof(wall.filename)));
	wall.pad = static_cast<char>(required_int(value, "Pad", -128, 127));
	Num_wall_anims = (std::max)(Num_wall_anims, index + 1);
}

void apply_patch_value(const patch_path &path, const json &value)
{
	if (path.section == "textures")
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
				if (value == op.end() || !value->is_object())
					throw std::runtime_error("HAM patch test operation is missing value object");
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
			if (value == op.end() || !value->is_object())
				throw std::runtime_error("HAM patch operation is missing value object");
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
