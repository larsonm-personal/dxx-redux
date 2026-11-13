#include "resource_validation.h"
#include "slow_motion_runtime.h"
#include "camera_collision.h"
#include "level_messages.h"
#include "../model_geometry.h"
#include "../model_hitbox.h"
#include "../model_hitbox_query.h"
#include "../model_sphere_query.h"
#include "../model_render_geometry.h"
#include "../camera_object_state.h"
#include <cstring>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <filesystem>
#include "picosha2.h"
#include "asset_dependencies.h"
#include "input_demo_assets.h"
#include "pog_resources.h"
#include "pog_renderer.h"
#include "named_audio.h"
#include "companion_resources.h"
#include "../inventory.h"
#include "preview.h"
#include "particle_resources.h"
#include "oof_resource.h"
#include "oof_renderer.h"
#include "../oof_geometry.h"
#include "../ase_mesh.h"
#include "../model_names.h"
#include "../particle_image.h"
#include "../time_equipment.h"
#include <utility>
#ifdef __ANDROID__
#include "../../app/src/main/cpp/shared/android_mission_assets.h"
#endif
extern "C" {
#include "inferno.h"
#include "bm.h"
#include "d2_ham_validation.h"
#include "piggy.h"
#include "physfsx.h"
#include "d1_custom_validation.h"
#include "u_mem.h"
#include "strutil.h"
#include "dxxerror.h"
#include "mission.h"
#include "palette.h"
#include "gauges.h"
#include "ai.h"
#include "weapon.h"
#include "console.h"
extern int Robot_replacements_loaded;
void bm_read_extra_robots(const char *name, int type);
}

namespace
{
static_assert(MAX_SUBMODELS == 10, "Update the bounded XL model geometry capacity with native submodels");
using File = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
std::string startup_ham_name, startup_ham_hash, startup_origin, base_rejection;
std::string vertigo_rejection, mod_rejection;
const std::array<const char *, 2> sound_banks{ "descent2.s11", "descent2.s22" };
std::array<std::string, 2> startup_sound_hashes;
std::map<std::string, std::string> startup_graphics_hashes;
bool mission_requires_xl = false;
std::map<int, std::shared_ptr<xl_compat::OofResource>> mission_oof_models;
std::map<std::string, std::string> mission_mod_sources;
std::map<std::string, std::string> mission_definition_sources;
std::set<int> mission_generated_models;

bool mounted_mission_requires_xl(bool owned_only = false)
{
	if (!Current_mission || Current_mission->descent_version != 2) return false;
	if (Current_mission->enhanced == MISSION_ENHANCED_XL) return true;
	auto extended = [owned_only](const char *filename) {
		std::string path = filename;
		if (!PHYSFSX_exists(path.c_str(), 1)) path = std::string(MISSION_DIR) + filename;
		if (owned_only && !mission_owns_resource_archive(PHYSFS_getRealDir(path.c_str()))) return false;
		File file(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
		unsigned char header[8];
		if (!file || PHYSFS_readBytes(file.get(), header, sizeof(header)) != sizeof(header)) return false;
		const auto version = uint32_t(header[4]) | uint32_t(header[5]) << 8 | uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
		return !std::memcmp(header, "LVLP", 4) && version > 8;
	};
	for (int i = 0; i < Last_level; ++i)
		if (extended(Level_names[i])) return true;
	for (int i = 0; i < -Last_secret_level; ++i)
		if (extended(Secret_level_names[i])) return true;
	return false;
}

std::string resource_hash(PHYSFS_file *file, const std::string &name)
{
	const auto position = PHYSFS_tell(file);
	const auto length = PHYSFS_fileLength(file);
	if (position < 0 || length <= 0 || length > 64 * 1024 * 1024 || !PHYSFS_seek(file, 0))
		throw std::runtime_error("base resource cannot be read within supported bounds: " + name);
	std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
	const auto read = PHYSFS_readBytes(file, bytes.data(), length);
	const bool restored = PHYSFS_seek(file, position) != 0;
	if (read != length || !restored) throw std::runtime_error("base resource read failed: " + name);
	return picosha2::hash256_hex_string(bytes);
}

std::string optional_resource_hash(const char *name)
{
	File file(PHYSFSX_openReadBuffered(name), PHYSFS_close);
	if (file) return resource_hash(file.get(), name);
	if (PHYSFSX_exists(name, 1)) throw std::runtime_error(std::string("cannot read base resource: ") + name);
	return {};
}

std::string resource_key(std::string name)
{
	for (char &c : name)
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return name;
}

struct MissionDefinitions {
	std::vector<robot_info> robots{ Robot_info, Robot_info + N_robot_types };
	std::vector<jointpos> joints{ Robot_joints, Robot_joints + N_robot_joints };
	std::vector<polymodel> models{ Polygon_models, Polygon_models + N_polygon_models };
	std::vector<int> dying{ Dying_modelnums, Dying_modelnums + N_polygon_models };
	std::vector<int> dead{ Dead_modelnums, Dead_modelnums + N_polygon_models };
	std::vector<bitmap_index> bitmaps{ ObjBitmaps, ObjBitmaps + MAX_OBJ_BITMAPS };
	std::vector<ushort> pointers{ ObjBitmapPtrs, ObjBitmapPtrs + MAX_OBJ_BITMAPS };
	std::vector<std::vector<ubyte>> payloads;
	std::vector<int> custom_radii;
	std::vector<int> base_radii;

	MissionDefinitions() : payloads(models.size()), custom_radii(models.size()), base_radii(models.size())
	{
		for (std::size_t id = 0; id < models.size(); ++id) {
			auto &model = models[id];
			base_radii[id] = model.rad;
			if (model.model_data && model.model_data_size > 0) {
				payloads[id].assign(model.model_data, model.model_data + model.model_data_size);
				model.model_data = payloads[id].data();
			}
		}
	}
};
std::unique_ptr<MissionDefinitions> mission_base;
std::unique_ptr<XlPreparedResources> prepared_resources;
// Native model tables belong to the last successful installation, which can
// differ from the level currently being prepared
XlPreparedResources *installed_resources = nullptr;
std::string prepared_level;

struct NativeModel {
	polymodel model;
	explicit NativeModel(const polymodel &source) : model(source)
	{
		model.model_data = static_cast<ubyte *>(d_malloc(source.model_data_size));
		if (!model.model_data) throw std::bad_alloc();
		std::memcpy(model.model_data, source.model_data, source.model_data_size);
		polygon_model_data_init(&model);
	}
	~NativeModel()
	{
		free_model(&model);
	}
};

File companion(const std::string &name)
{
	// Match the native HXM/POG loaders' mounted lookup exactly
	File file(PHYSFSX_openReadBuffered(name.c_str()), PHYSFS_close);
	if (!file && PHYSFSX_exists(name.c_str(), 1))
		throw std::runtime_error("cannot read level resource: " + name);
	return file;
}

struct HxmChanges {
	std::set<int> robots, joints, models, bitmaps, pointers;
};

std::pair<int, int> validate_hxm_dependencies(PHYSFS_file *file, MissionDefinitions &definitions, xl_assets::ObjectDependencies &graph, HxmChanges &changes, bool camera = false, bool defer_dependencies = false, bool defer_robots = false)
{
	// Native readers decode into the same owned tables used by the level walk
	auto &robots = definitions.robots;
	auto &joints = definitions.joints;
	auto &models = definitions.models;
	auto &dying = definitions.dying;
	auto &dead = definitions.dead;
	auto &bitmaps = definitions.bitmaps;
	auto &pointers = definitions.pointers;
	auto &payloads = definitions.payloads;
	auto &changed_robots = changes.robots;
	auto &changed_joints = changes.joints;
	auto &changed_models = changes.models;
	auto &changed_bitmaps = changes.bitmaps;
	auto &changed_pointers = changes.pointers;
	std::pair<int, int> camera_ids{ -1, -1 };
	PHYSFS_seek(file, 8);
	int count = PHYSFSX_readInt(file);
	if (camera && (count != 1 || robots.size() >= MAX_ROBOT_TYPES)) throw std::runtime_error("Camera addon requires one appended robot within native capacity");
	for (; count > 0; --count) {
		int id = PHYSFSX_readInt(file);
		if (camera) {
			id = int(robots.size());
			robots.emplace_back();
			camera_ids.first = id;
		}
		robot_info_read_n(&robots.at(id), 1, file);
		static_assert(sizeof(jointlist) == 4, "XL HXM joint-list storage must match the serialized pair");
		if (!d2_ham_normalize_xl_unused_joint_lists(
		        reinterpret_cast<uint8_t *>(&robots.at(id).anim_states[0][0]), robots.at(id).n_guns))
			throw std::runtime_error("HXM robot gun count invalid");
		changed_robots.insert(id);
	}
	count = PHYSFSX_readInt(file);
	if (camera && count) throw std::runtime_error("Animated camera addon joints are not qualified");
	for (; count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		jointpos_read_n(&joints.at(id), 1, file);
		changed_joints.insert(id);
	}
	count = PHYSFSX_readInt(file);
	if (camera && (count != 1 || models.size() >= MAX_POLYGON_MODELS)) throw std::runtime_error("Camera addon requires one appended model within native capacity");
	for (; count > 0; --count) {
		int id = PHYSFSX_readInt(file);
		if (camera) {
			id = int(models.size());
			camera_ids.second = id;
			models.emplace_back();
			payloads.emplace_back();
			definitions.custom_radii.push_back(0);
			definitions.base_radii.push_back(0);
			dying.push_back(-1);
			dead.push_back(-1);
		}
		auto &model = models.at(id);
		const int base_radius = definitions.base_radii.at(id);
		polymodel_read(&model, file);
		auto &payload = payloads.at(id);
		payload.resize(model.model_data_size);
		if (PHYSFS_read(file, payload.data(), 1, static_cast<PHYSFS_uint32>(payload.size())) != static_cast<PHYSFS_sint64>(payload.size()))
			throw std::runtime_error("HXM model payload read failed");
		model.model_data = payload.data();
		definitions.custom_radii.at(id) = xl_compat::ModelGeometry(payload.data(), payload.size(), model.n_models).custom_radius();
		// XL keeps the base radius across replacements; projectiles request slot 1
		model.rad = base_radius ? base_radius : definitions.custom_radii.at(id);
		dying.at(id) = PHYSFSX_readInt(file);
		dead.at(id) = PHYSFSX_readInt(file);
		changed_models.insert(id);
	}
	for (int count = PHYSFSX_readInt(file); count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		bitmap_index_read(&bitmaps.at(id), file);
		changed_bitmaps.insert(id);
	}
	for (int count = PHYSFSX_readInt(file); count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		pointers.at(id) = PHYSFSX_readShort(file);
		changed_pointers.insert(id);
	}
	graph.robot_data = robots.data();
	graph.joint_data = joints.data();
	graph.model_data = models.data();
	graph.object_bitmaps = bitmaps.data();
	graph.object_bitmap_ptrs = pointers.data();
	graph.dying_models = dying.data();
	graph.dead_models = dead.data();
	graph.robot_count = int(robots.size());
	graph.joint_count = int(joints.size());
	graph.model_count = int(models.size());
	if (camera) {
		// Pinned InitCamBots normalizes the appended definition, not serialized slot 38
		auto &robot = robots.at(camera_ids.first);
		robot.model_num = camera_ids.second;
		robot.attack_type = 0;
		robot.contains_id = robot.contains_count = robot.contains_prob = robot.contains_type = 0;
		robot.score_value = 0;
		robot.strength = -1;
		robot.mass = F1_0 / 2;
		robot.drag = 0;
		robot.see_sound = robot.attack_sound = robot.claw_sound = robot.taunt_sound = 0;
		robot.behavior = 128;
		robot.aim = 0;
		// Indexed writes avoid binding C++ array references to packed robot fields
		for (int difficulty = 0; difficulty < NDL; ++difficulty) {
			robot.turn_time[difficulty] = 0;
			robot.max_speed[difficulty] = 0;
			robot.circle_distance[difficulty] = 0;
			robot.rapidfire_count[difficulty] = 0;
		}
		if (models[camera_ids.second].n_models != 1 || models[camera_ids.second].simpler_model ||
		    dying[camera_ids.second] != -1 || dead[camera_ids.second] != -1)
			throw std::runtime_error("Camera addon profile: guns=" + std::to_string(robot.n_guns) + ", submodels=" + std::to_string(models[camera_ids.second].n_models) +
			                         ", simpler=" + std::to_string(models[camera_ids.second].simpler_model) + ", dying=" + std::to_string(dying[camera_ids.second]) + ", dead=" + std::to_string(dead[camera_ids.second]));
	}
	// Mission HXM is followed by V-HAM; validate reachable closure after all layers
	if (defer_dependencies) return camera_ids;
	for (const int id : changed_models) graph.model(id);
	for (const int id : changed_bitmaps) graph.bitmap(bitmaps[id].index);
	for (const int id : changed_pointers) {
		if (pointers[id] >= N_ObjBitmaps) throw std::runtime_error("HXM model texture references missing object bitmap");
		graph.bitmap(bitmaps[pointers[id]].index);
	}
	// XL level admission walks reachable robot profiles after HXM and model layers
	if (defer_robots) return camera_ids;
	// A changed joint/model can invalidate an otherwise unchanged robot
	for (int id = 0; id < int(robots.size()); ++id) {
		// Camera control never dispatches ordinary robot guns, combat or animation
		if (id == camera_ids.first) continue;
		bool affected = changed_robots.count(id) || changed_models.count(robots[id].model_num);
		for (int gun = 0; gun <= MAX_GUNS; ++gun)
			for (int animation = 0; animation < N_ANIM_STATES; ++animation) {
				const auto state = robots[id].anim_states[gun][animation];
				for (const int joint : changed_joints)
					if (joint >= state.offset && joint - state.offset < state.n_joints) affected = true;
			}
		if (affected) graph.robot(id);
	}
	return camera_ids;
}

struct CameraAddonFile {
	static constexpr const char *mount = "__xl_camera_resources__.hog";
	File file{ nullptr, PHYSFS_close };
	bool mounted = false;
	~CameraAddonFile()
	{
		file.reset();
		if (mounted) PHYSFS_unmount(mount);
	}
	static bool owned(std::string &path)
	{
		if (PHYSFSEXT_locateCorrectCase(path.data()) != 0) return false;
		const char *origin = PHYSFS_getRealDir(path.c_str());
		// Android mounts the selected mission's parent directory separately after
		// recording the descriptor search root, so these two owned roots can differ
		const auto directory = [](const std::filesystem::path &value) {
			auto result = value.lexically_normal().generic_string();
			while (result.size() > 1 && result.back() == '/') result.pop_back();
			return result;
		};
		const bool selected_parent = origin && *mission_resource_archive() &&
		                             directory(origin) == directory(std::filesystem::path(mission_resource_archive()).parent_path());
		if (!selected_parent && !mission_owns_resource_directory(origin) && !mission_owns_resource_archive(origin) && !xl_resource_owns_installation(origin))
			throw std::runtime_error("Camera addon is shadowed outside the selected mission or base installation: " + path);
		return true;
	}
	CameraAddonFile()
	{
		std::string path = "cambot.hxm";
		if (owned(path)) file.reset(PHYSFS_openRead(path.c_str()));
		else {
			path = "data/d2x-xl.hog";
			if (!owned(path)) {
				path = "d2x-xl.hog";
				if (!owned(path)) throw std::runtime_error("Missing owned cambot.hxm or data/d2x-xl.hog camera addon. Import your XL resources and enable the XL resources mod in the launcher");
			}
			if (PHYSFS_getMountPoint(mount)) throw std::runtime_error("Camera addon private mount is already active");
			File archive(PHYSFS_openRead(path.c_str()), PHYSFS_close);
			if (!archive || !PHYSFS_mountHandle(archive.get(), mount, "__xl_camera_resources__", 0))
				throw std::runtime_error("Cannot mount owned camera addon archive");
			archive.release();
			mounted = true;
			file.reset(PHYSFSX_openReadBuffered("__xl_camera_resources__/cambot.hxm"));
		}
		// Constructor failure must release the temporary mount as well as its member
		if (!file) {
			if (mounted) {
				PHYSFS_unmount(mount);
				mounted = false;
			}
			throw std::runtime_error("Camera addon model is missing");
		}
	}
};

} // namespace

extern "C" void xl_resource_note_startup_ham(const char *name, PHYSFS_file *file)
{
	try {
		startup_ham_hash = resource_hash(file, name);
		startup_ham_name = name;
		auto corrected = startup_ham_name;
		const char *origin = PHYSFSEXT_locateCorrectCase(corrected.data()) == 0 ? PHYSFS_getRealDir(corrected.c_str()) : nullptr;
		startup_origin = origin ? origin : "";
		// Both banks matter: playback may fall back and authored durations use the canonical bank
		for (std::size_t i = 0; i < sound_banks.size(); ++i)
			startup_sound_hashes[i] = optional_resource_hash(sound_banks[i]);
		startup_graphics_hashes.clear();
		std::unique_ptr<char *, decltype(&PHYSFS_freeList)> files(PHYSFS_enumerateFiles(""), PHYSFS_freeList);
		if (!files) throw std::runtime_error("cannot enumerate startup graphics resources");
		for (char **entry = files.get(); *entry; ++entry) {
			const auto key = resource_key(*entry);
			const auto dot = key.find_last_of('.');
			if (dot != std::string::npos && (key.substr(dot) == ".256" || key.substr(dot) == ".pig"))
				startup_graphics_hashes.emplace(key, optional_resource_hash(*entry));
		}
	} catch (...) {
		startup_ham_hash.clear();
		startup_ham_name.clear();
		startup_origin.clear();
		startup_sound_hashes = {};
		startup_graphics_hashes.clear();
	}
}

extern "C" void xl_resource_select_mission(int requires_xl)
{
	xl_oof_renderer_clear();
	mission_oof_models.clear();
	mission_mod_sources.clear();
	mission_definition_sources.clear();
	mission_generated_models.clear();
	mission_requires_xl = requires_xl != 0;
	base_rejection.clear();
	vertigo_rejection.clear();
	mod_rejection.clear();
}

// Pinned LoadModData stage zero tries the original Vertigo extension for D2
// Use the existing validated decoder, without admitting arbitrary custom HAMs
extern "C" int xl_resource_load_vertigo(void)
{
	if (!mission_requires_xl || !Current_mission || Current_mission->descent_version != 2) return 0;
	vertigo_rejection.clear();
	constexpr const char *mount = "__xl_vertigo_resources__.hog";
	bool mounted = false;
	try {
		std::string path;
		for (const char *candidate : { "d2x.ham", "data/d2x.ham" }) {
			std::string name = candidate;
			if (CameraAddonFile::owned(name)) {
				path = name;
				break;
			}
		}
		if (path.empty()) {
			for (const char *candidate : { "d2x.hog", "missions/d2x.hog", "data/d2x.hog" }) {
				std::string name = candidate;
				if (!CameraAddonFile::owned(name)) continue;
				if (PHYSFS_getMountPoint(mount)) throw std::runtime_error("Vertigo private mount is already active");
				File archive(PHYSFS_openRead(name.c_str()), PHYSFS_close);
				input_demo_assets_note_container(mount, name.c_str(), archive.get());
				if (!archive || !PHYSFS_mountHandle(archive.get(), mount, "__xl_vertigo_resources__", 0))
					throw std::runtime_error("Cannot mount owned Vertigo archive");
				archive.release();
				mounted = true;
				path = "__xl_vertigo_resources__/d2x.ham";
				break;
			}
		}
		if (path.empty()) return 0;
		{
			File file(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
			if (!file || resource_hash(file.get(), path) != "faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f")
				throw std::runtime_error("XL requires the qualified original Vertigo d2x.ham");
		}
		bm_read_extra_robots(path.c_str(), 2);
		if (N_robot_types != 78 || Robot_info[68].model_num < 0 || Robot_info[76].model_num < 0)
			throw std::runtime_error("Original Vertigo robot extension failed validated loading");
		if (mounted) PHYSFS_unmount(mount);
		return 1;
	} catch (const std::exception &error) {
		if (mounted) PHYSFS_unmount(mount);
		vertigo_rejection = error.what();
		con_printf(CON_URGENT, "XL Vertigo resources rejected: %s\n", error.what());
		return -1;
	}
}

namespace
{
std::string owned_mod_file(const char *extension, const std::string &relative = {})
{
	const std::string archive = mission_resource_archive();
	const auto slash = archive.find_last_of("/\\");
	const auto leaf = archive.substr(slash == std::string::npos ? 0 : slash + 1);
	const auto dot = leaf.find_last_of('.');
	if (dot == std::string::npos || resource_key(leaf.substr(dot)) != ".hog") return {};
	const auto name = leaf.substr(0, dot);
	if (name.empty() || name == "." || name == ".." || name.find_first_of(":/\\") != std::string::npos)
		throw std::runtime_error("Invalid XL mission resource identity");
	std::string path = "mods/" + name + "/" + (relative.empty() ? name + extension : relative);
#ifdef __ANDROID__
	if (*android_mission_assets_key()) {
		char resolved[PATH_MAX];
		if (!android_mission_assets_find_owned_file(path.c_str(), resolved, sizeof(resolved))) return {};
		path = resolved;
	}
#endif
	if (PHYSFSEXT_locateCorrectCase(path.data()) != 0) return {};
	const char *origin = PHYSFS_getRealDir(path.c_str());
#ifdef __ANDROID__
	if (!*android_mission_assets_key())
#endif
		if (!mission_owns_resource_directory(origin) && !mission_owns_resource_archive(origin))
			throw std::runtime_error("XL mission definitions are shadowed outside their owner: " + path);
	return path;
}

void install_mission_hxm(PHYSFS_file *file)
{
	if (!bm_validate_level_hxm(file)) throw std::runtime_error("Malformed XL mission HXM");
	MissionDefinitions definitions;
	xl_assets::ObjectDependencies graph;
	HxmChanges changes;
	validate_hxm_dependencies(file, definitions, graph, changes, false, true);
	std::map<int, std::unique_ptr<NativeModel>> models;
	for (int id : changes.models) models[id] = std::make_unique<NativeModel>(definitions.models[id]);
	// All decoding and allocations finish before native tables change
	for (int id : changes.robots) Robot_info[id] = definitions.robots[id];
	for (int id : changes.joints) Robot_joints[id] = definitions.joints[id];
	for (int id : changes.bitmaps) ObjBitmaps[id] = definitions.bitmaps[id];
	for (int id : changes.pointers) ObjBitmapPtrs[id] = definitions.pointers[id];
	for (auto &entry : models) {
		const int id = entry.first;
		free_model(&Polygon_models[id]);
		Polygon_models[id] = entry.second->model;
		entry.second->model.model_data = nullptr;
		Dying_modelnums[id] = definitions.dying[id];
		Dead_modelnums[id] = definitions.dead[id];
	}
}

std::vector<uint8_t> read_model_resource(const std::string &path)
{
	File file(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length <= 0 || length > 64 * 1024 * 1024) throw std::runtime_error("Invalid owned model resource: " + path);
	std::vector<uint8_t> bytes(static_cast<std::size_t>(length));
	if (PHYSFS_readBytes(file.get(), bytes.data(), length) != length) throw std::runtime_error("Cannot read owned model resource: " + path);
	return bytes;
}

xl_compat::OwnedModelSet prepare_owned_models(int ordinal)
{
	xl_compat::OwnedModelSet result;
	auto &pending = result.models;
	auto &sources = result.sources;
	std::vector<std::string> folders;
	if (ordinal) {
		const auto number = std::to_string(ordinal < 0 ? -ordinal : ordinal);
		folders.push_back(std::string("models/") + (ordinal < 0 ? "slevel" : "level") + (number.size() < 2 ? "0" : "") + number + "/");
	}
	folders.push_back("models/");
	std::map<std::string, std::shared_ptr<xl_compat::OofResource>> by_path;
	std::map<std::string, std::shared_ptr<const xl_compat::PogBitmap>> skins_by_path;
	for (int id = 0; id < MAX_POLYGON_MODELS; ++id) {
		const auto mapping = xl_compat::model_name(id);
		if (mapping.shared >= 0) {
			const auto shared = pending.find(mapping.shared);
			if (shared != pending.end()) {
				pending.emplace(id, shared->second);
				continue;
			}
		}
		std::string path;
		std::string selected_folder;
		for (const auto &folder : folders) {
			for (const auto &candidate : xl_compat::model_candidates(id)) {
				path = owned_mod_file("", folder + candidate);
				if (!path.empty()) {
					selected_folder = folder;
					break;
				}
			}
			if (!path.empty()) break;
		}
		if (path.empty()) continue;
		const bool ase = resource_key(path).substr(path.size() - 4) == ".ase";
		const auto cached = by_path.find(path);
		if (cached != by_path.end()) {
			pending.emplace(id, cached->second);
			continue;
		}
		auto resource = std::make_shared<xl_compat::OofResource>();
		resource->path = path;
		const auto model_bytes = read_model_resource(path);
		if (ase) {
			const std::string text(model_bytes.begin(), model_bytes.end());
			auto compiled = xl_compat::compile_ase_mesh(xl_compat::decode_ase_model(xl_compat::read_ase_records(text)));
			resource->model.textures = std::move(compiled.textures);
			resource->mesh = std::move(compiled.geometry);
			resource->gunpoints = std::move(compiled.gunpoints);
			resource->thrusters = std::move(compiled.thrusters);
		} else {
			resource->model = xl_compat::read_oof(model_bytes);
			resource->mesh = xl_compat::compile_oof_mesh(resource->model, mapping.flip_v);
		}
		sources[path] = picosha2::hash256_hex_string(model_bytes);
		for (auto texture : resource->model.textures) {
			if (texture.empty() || texture == "." || texture == ".." || texture.find_first_of("/\\:") != std::string::npos)
				throw std::runtime_error("Invalid owned OOF skin name: " + path);
			if (resource_key(texture).size() < 4 || resource_key(texture).substr(texture.size() - 4) != ".tga") texture += ".tga";
			auto skin_path = owned_mod_file("", selected_folder + texture);
			if (skin_path.empty() && selected_folder != "models/") skin_path = owned_mod_file("", "models/" + texture);
			if (skin_path.empty()) throw std::runtime_error("Missing owned OOF skin: " + texture);
			auto &skin = skins_by_path[skin_path];
			if (!skin) {
				const auto bytes = read_model_resource(skin_path);
				skin = std::make_shared<const xl_compat::PogBitmap>(xl_compat::read_particle_image(bytes.data(), bytes.size()));
				sources[skin_path] = picosha2::hash256_hex_string(bytes);
			}
			resource->skins.push_back(skin);
		}
		by_path.emplace(path, resource);
		pending.emplace(id, std::move(resource));
	}
	return result;
}

void install_missing_oof_geometry()
{
	std::map<int, std::unique_ptr<NativeModel>> pending;
	for (const auto &entry : mission_oof_models) {
		if (entry.first < N_polygon_models && Polygon_models[entry.first].model_data) continue;
		const auto payload = xl_compat::oof_native_geometry(entry.second->mesh);
		polymodel model{};
		model.n_models = 1;
		model.model_data_size = int(payload.size());
		model.model_data = const_cast<ubyte *>(payload.data());
		model.rad = xl_compat::ModelGeometry(payload.data(), payload.size(), 1).custom_radius();
		model.submodel_rads[0] = model.rad;
		model.mins = { fix(entry.second->mesh.low[0] * 65536), fix(entry.second->mesh.low[1] * 65536), fix(entry.second->mesh.low[2] * 65536) };
		model.maxs = { fix(entry.second->mesh.high[0] * 65536), fix(entry.second->mesh.high[1] * 65536), fix(entry.second->mesh.high[2] * 65536) };
		model.submodel_mins[0] = model.mins;
		model.submodel_maxs[0] = model.maxs;
		pending.emplace(entry.first, std::make_unique<NativeModel>(model));
		mission_generated_models.insert(entry.first);
	}
	for (auto &entry : pending) {
		while (N_polygon_models <= entry.first) {
			Polygon_models[N_polygon_models] = {};
			Dying_modelnums[N_polygon_models] = Dead_modelnums[N_polygon_models] = -1;
			++N_polygon_models;
		}
		Polygon_models[entry.first] = entry.second->model;
		entry.second->model.model_data = nullptr;
	}
}
} // namespace

std::shared_ptr<const xl_compat::OofResource> xl_owned_oof_model(int id)
{
	const auto found = mission_oof_models.find(id);
	return found == mission_oof_models.end() ? nullptr : found->second;
}

xl_compat::OwnedModelSet xl_prepare_owned_models(int ordinal)
{
	if (!mission_requires_xl || !Current_mission || Current_mission->descent_version != 2) return {};
	if (ordinal > Last_level || ordinal < Last_secret_level) throw std::runtime_error("Invalid XL model level ordinal");
	return prepare_owned_models(ordinal);
}

extern "C" int xl_resource_load_mod_data(void)
{
	if (!mission_requires_xl || !Current_mission || Current_mission->descent_version != 2) return 0;
	mod_rejection.clear();
	try {
		std::map<std::string, std::string> sources;
		const auto ham = owned_mod_file(".ham"), hxm = owned_mod_file(".hxm"), vham = owned_mod_file(".vham");
		for (const auto &path : { ham, hxm, vham }) {
			if (path.empty()) continue;
			File file(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
			if (!file) throw std::runtime_error("Cannot read XL mission definition: " + path);
			sources[path] = resource_hash(file.get(), path);
		}
		if (!ham.empty()) {
			File file(PHYSFSX_openReadBuffered(ham.c_str()), PHYSFS_close);
			if (!bm_read_xl_main_ham(file.get())) throw std::runtime_error("Malformed or unsupported XL mission HAM: " + ham);
			input_demo_assets_extra_ham_loaded(ham.c_str(), file.get());
		}
		if (!hxm.empty()) {
			File file(PHYSFSX_openReadBuffered(hxm.c_str()), PHYSFS_close);
			install_mission_hxm(file.get());
		}
		if (!vham.empty() && !bm_read_xl_extra_robots(vham.c_str()))
			throw std::runtime_error("Malformed XL mission V-HAM: " + vham);
		auto staged_models = xl_prepare_owned_models(0);
		mission_definition_sources = sources;
		sources.insert(staged_models.sources.begin(), staged_models.sources.end());
		mission_oof_models = std::move(staged_models.models);
		install_missing_oof_geometry();
		mission_mod_sources = std::move(sources);
		if (ham.empty() && hxm.empty() && vham.empty() && mission_oof_models.empty()) return 0;
		con_printf(CON_NORMAL, "XL mission definitions: HAM=%s HXM=%s VHAM=%s robots=%d models=%d\n",
		           ham.c_str(), hxm.c_str(), vham.c_str(), N_robot_types, N_polygon_models);
		return 1;
	} catch (const std::exception &error) {
		// The caller has already reset to retail data; failed layers must not survive
		mod_rejection = error.what();
		mission_oof_models.clear();
		mission_mod_sources.clear();
		mission_definition_sources.clear();
		mission_generated_models.clear();
		piggy_load_mission_data();
		con_printf(CON_URGENT, "XL mission definitions rejected: %s\n", mod_rejection.c_str());
		return -1;
	}
}

const std::map<std::string, std::string> &xl_resource_mod_sources()
{
	return mission_mod_sources;
}

std::map<std::string, std::string> xl_resource_sources_for_level(int ordinal)
{
	auto result = mission_definition_sources;
	const auto models = xl_prepare_owned_models(ordinal);
	result.insert(models.sources.begin(), models.sources.end());
	return result;
}

extern "C" int xl_model_gun_point(int model, int gun, vms_vector *point)
{
	if (!mission_requires_xl || !point || gun < 0 || gun >= 8) return 0;
	const auto resource = xl_owned_oof_model(model);
	if (!resource || resource->gunpoints.empty()) return 0;
	// XL selects the model's complete gun array when it has any authored markers
	*point = {};
	const auto found = resource->gunpoints.find(gun);
	if (found != resource->gunpoints.end()) {
		point->x = fix(found->second[0] * 65536);
		point->y = fix(found->second[1] * 65536);
		point->z = fix(found->second[2] * 65536);
	}
	return 1;
}

extern "C" void xl_resource_commit_mission(void)
{
	// Seal from the parsed descriptor or selected HOG, never from texture presence
	mission_requires_xl = mounted_mission_requires_xl(true);
}

extern "C" int xl_resource_texture_scope_enabled(void)
{
	return mission_requires_xl;
}

extern "C" int xl_resource_owns_installation(const char *source)
{
#ifdef __ANDROID__
	// Launcher-owned file-set projections carry installed addons separately from retail HAM
	if (android_mission_assets_owns_installation(source)) return 1;
#endif
	return source && !startup_origin.empty() && startup_origin == source;
}

extern "C" const char *xl_resource_base_rejection(void)
{
	base_rejection.clear();
	try {
		// The browser flag is only a hint; mounted level headers remain authoritative
		if (!mission_requires_xl && !mounted_mission_requires_xl()) return base_rejection.c_str();
		if (!vertigo_rejection.empty()) throw std::runtime_error(vertigo_rejection);
		if (!mod_rejection.empty()) throw std::runtime_error(mod_rejection);
		if (startup_ham_hash.empty()) throw std::runtime_error("trusted startup base HAM is unavailable for XL resource admission");
		File file(PHYSFSX_openReadBuffered(startup_ham_name.c_str()), PHYSFS_close);
		if (!file) throw std::runtime_error("required base HAM is missing: " + startup_ham_name);
		if (resource_hash(file.get(), startup_ham_name) != startup_ham_hash)
			throw std::runtime_error("mission overrides base HAM definitions: custom HAM gameplay support is unqualified; previous resources were retained");
		for (std::size_t i = 0; i < sound_banks.size(); ++i)
			if (optional_resource_hash(sound_banks[i]) != startup_sound_hashes[i])
				throw std::runtime_error(std::string("mission changes base sound bank ") + sound_banks[i] + ": custom sound-bank initialization is unqualified; previous resources were retained");
		// Descriptor initialization may transition from D1 and reset the default graphics registry
		if (Current_mission && Current_mission->enhanced == MISSION_ENHANCED_XL)
			xl_validate_level_graphics(DEFAULT_LEVEL_PALETTE);
	} catch (const std::exception &error) {
		base_rejection = error.what();
	}
	return base_rejection.c_str();
}

void xl_validate_level_graphics(const std::string &palette)
{
	// Match the stock fixed-size palette name and flat basename-to-PIG selection
	if (palette.empty() || palette.size() >= FILENAME_LEN || palette.find_first_of("/\\:") != std::string::npos)
		throw std::runtime_error("unsupported level palette name: " + palette);
	const auto name = resource_key(palette);
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos || name.substr(dot) != ".256")
		throw std::runtime_error("unsupported level palette extension: " + palette);
	const auto pig = name.substr(0, dot) + ".pig";
	for (const auto &resource : { name, pig }) {
		const auto trusted = startup_graphics_hashes.find(resource);
		if (trusted == startup_graphics_hashes.end() || trusted->second.empty())
			throw std::runtime_error("unqualified graphics resource: " + resource + "; no trusted startup definition");
		const auto current = optional_resource_hash(resource.c_str());
		if (current.empty()) throw std::runtime_error("required graphics resource is missing: " + resource);
		if (current != trusted->second)
			throw std::runtime_error("mission changes graphics resource " + resource + ": custom palette/PIG initialization is unqualified");
	}
}

extern "C" void xl_resource_baseline_clear(void)
{
	installed_resources = nullptr;
	mission_base.reset();
}

extern "C" void xl_resource_baseline_capture(void)
{
	installed_resources = nullptr;
	// Called after mission HAM initialization, before level replacements
	mission_base.reset();
	try {
		mission_base = std::make_unique<MissionDefinitions>();
	} catch (...) {
		// No exception may cross the native mission loader; preflight fails closed
	}
}

struct XlPreparedResources::State {
	xl_compat::OwnedModelSet owned_models;
	MissionDefinitions definitions;
	xl_assets::ObjectDependencies graph;
	bool has_hxm = false;
	bool installed = false;
	int camera_robot = -1, camera_model = -1;
	HxmChanges changes;
	xl_compat::MissionLightingCompanions lighting;
	std::shared_ptr<const xl_compat::MessageResources> messages;
	std::shared_ptr<const xl_compat::AudioResources> audio;
	std::shared_ptr<const xl_compat::ParticleImageResource> particle_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> smoke_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> bubble_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> rain_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> snow_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> lightning_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> powerup_glare_image;
	std::array<std::shared_ptr<const xl_compat::AnimatedImageResource>, 2> time_equipment_images;
	std::vector<std::unique_ptr<NativeModel>> native_models;
	struct CollisionModel {
		xl_compat::RenderModelSphere sphere;
		xl_compat::ModelHitboxTransform transform;
		explicit CollisionModel(const xl_compat::ModelRenderGeometry &geometry) : sphere(geometry.bounding_sphere()), transform(geometry.hitbox_bounds()) {}
	};
	std::vector<std::unique_ptr<CollisionModel>> hitboxes;
	int hitbox_frame = 1;
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> pog{ nullptr, xl_pog_free };

	void prepare_native_models()
	{
		native_models.resize(definitions.models.size());
		for (const int id : changes.models)
			native_models[id] = std::make_unique<NativeModel>(definitions.models[id]);
	}

	void prepare_models(int ordinal)
	{
		owned_models = xl_prepare_owned_models(ordinal);
		owned_models.sources.insert(mission_definition_sources.begin(), mission_definition_sources.end());
		for (const auto &entry : owned_models.models) {
			const int id = entry.first;
			if (changes.models.count(id)) continue;
			if (std::size_t(id) < definitions.models.size() && definitions.models[id].model_data && !mission_generated_models.count(id)) continue;
			const std::size_t size = (std::max) (definitions.models.size(), std::size_t(id + 1));
			definitions.models.resize(size);
			definitions.payloads.resize(size);
			definitions.dying.resize(size, -1);
			definitions.dead.resize(size, -1);
			definitions.custom_radii.resize(size);
			definitions.base_radii.resize(size);
			auto &payload = definitions.payloads[id];
			payload = xl_compat::oof_native_geometry(entry.second->mesh);
			auto &model = definitions.models[id];
			model = {};
			model.n_models = 1;
			model.model_data_size = int(payload.size());
			model.model_data = payload.data();
			model.rad = xl_compat::ModelGeometry(payload.data(), payload.size(), 1).custom_radius();
			model.submodel_rads[0] = model.rad;
			const auto &mesh = entry.second->mesh;
			model.mins = { fix(mesh.low[0] * 65536), fix(mesh.low[1] * 65536), fix(mesh.low[2] * 65536) };
			model.maxs = { fix(mesh.high[0] * 65536), fix(mesh.high[1] * 65536), fix(mesh.high[2] * 65536) };
			model.submodel_mins[0] = model.mins;
			model.submodel_maxs[0] = model.maxs;
			definitions.custom_radii[id] = model.rad;
			changes.models.insert(id);
			has_hxm = true;
		}
		bind_graph();
		prepare_native_models();
	}

	explicit State(const MissionDefinitions &base) : definitions(base)
	{
		// Vector copying owns bytes, but model headers still need their pointers rebound
		for (std::size_t id = 0; id < definitions.models.size(); ++id)
			if (!definitions.payloads[id].empty())
				definitions.models[id].model_data = definitions.payloads[id].data();
		bind_graph();
	}
	void bind_graph()
	{
		graph = xl_assets::ObjectDependencies{};
		graph.robot_data = definitions.robots.data();
		graph.joint_data = definitions.joints.data();
		graph.model_data = definitions.models.data();
		graph.object_bitmaps = definitions.bitmaps.data();
		graph.object_bitmap_ptrs = definitions.pointers.data();
		graph.dying_models = definitions.dying.data();
		graph.dead_models = definitions.dead.data();
		graph.robot_count = int(definitions.robots.size());
		graph.joint_count = int(definitions.joints.size());
		graph.model_count = int(definitions.models.size());
		graph.xl_boss_spawning = true;
	}
};

XlPreparedResources::~XlPreparedResources()
{
	if (installed_resources == this) installed_resources = nullptr;
}

xl_assets::ObjectDependencies &XlPreparedResources::dependencies()
{
	return state->graph;
}

XlPreparedResources::XlPreparedResources(const char *level_filename, const xl_compat::Inventory *level, int ordinal)
{
	if (!mission_base) throw std::runtime_error("clean mission definitions are unavailable");
	const auto counts_match = [](const MissionDefinitions &definitions) {
		return definitions.robots.size() == std::size_t(N_robot_types) && definitions.joints.size() == std::size_t(N_robot_joints) && definitions.models.size() == std::size_t(N_polygon_models);
	};
	if (!counts_match(*mission_base) && !(prepared_resources && prepared_resources->state->installed && counts_match(prepared_resources->state->definitions)))
		throw std::runtime_error("definition registry changed outside the owned mission resources");
	state = std::make_unique<State>(*mission_base);
	if (!ordinal) {
		for (int candidate = Last_secret_level; candidate <= Last_level; ++candidate) {
			if (!candidate) continue;
			const char *name = candidate < 0 ? Secret_level_names[-candidate - 1] : Level_names[candidate - 1];
			if (d_stricmp(level_filename, name)) continue;
			if (ordinal) throw std::runtime_error("XL resources require an explicit incoming level number for a repeated level filename");
			ordinal = candidate;
		}
	}
	if (level) state->messages = xl_compat::prepare_messages(level_filename, *level);
	if (level) state->audio = xl_compat::prepare_named_audio(level_filename, *level, ordinal);
	if (level) {
		state->particle_image = xl_compat::prepare_particle_image(*level);
		state->smoke_image = xl_compat::prepare_particle_image(*level, 0);
		state->bubble_image = xl_compat::prepare_particle_image(*level, 2);
		state->rain_image = xl_compat::prepare_particle_image(*level, 5);
		state->snow_image = xl_compat::prepare_particle_image(*level, 6);
		state->lightning_image = xl_compat::prepare_lightning_image(*level);
		state->powerup_glare_image = xl_compat::prepare_powerup_glare_image();
		if (xl_runtime::time_equipment_resources_required(*level))
			for (unsigned i = 0; i < 2; ++i) state->time_equipment_images[i] = xl_compat::prepare_time_equipment_image(i);
	}
	std::string base = level_filename;
	const auto dot = base.find_last_of('.');
	if (dot != std::string::npos) base.resize(dot);
	if (PHYSFSX_exists((base + ".lgt").c_str(), 1) || PHYSFSX_exists((base + ".clr").c_str(), 1)) {
		if (!*mission_resource_archive())
			throw std::runtime_error("D2 XL lighting companions require an owned mission HOG: " + base);
		state->lighting = xl_compat::load_mission_lighting_companions(level_filename);
		// The pinned modern loader reads CLR first, then replaces every texture color
		// from the embedded table. Ownership and decoding still apply to the companion
		if (state->lighting.colors && !(level && xl_compat::modern_runtime_layout(*level) && level->texture_colors.size() == xl_compat::companion_texture_count))
			throw std::runtime_error("D2 XL CLR companion overrides are not supported: " + std::string(level_filename));
	}
	const auto hxm_name = base + ".HXM";
	if (auto file = companion(hxm_name)) {
		if (!bm_validate_xl_level_hxm(file.get()))
			throw std::runtime_error("HXM definitions are malformed or require unsupported normalization: " + hxm_name);
		try {
			validate_hxm_dependencies(file.get(), state->definitions, state->graph, state->changes, false, false, true);
			state->has_hxm = true;
			state->prepare_native_models();
		} catch (const std::exception &error) {
			throw std::runtime_error("HXM dependencies: " + hxm_name + ": " + error.what());
		}
	}
	state->prepare_models(ordinal);
	const auto pog_name = base + ".POG";
	if (auto file = companion(pog_name)) {
		char error[256]{};
		state->pog.reset(xl_pog_prepare(file.get(), Num_bitmap_files, error, sizeof(error)));
		if (!state->pog) throw std::runtime_error(pog_name + ": " + error);
		for (int i = 0; i < xl_pog_count(state->pog.get()); ++i) {
			const auto *entry = xl_pog_entry(state->pog.get(), i);
			if (!entry->rgba) continue;
			if (entry->frames > 1) {
				const int clock = xl_pog_strip_clock(entry->index);
				if (clock < 0) throw std::runtime_error(pog_name + ": native bitmap animation tables exceed supported bounds");
				if (clock) throw std::runtime_error(pog_name + ": RGBA bitmap " + std::to_string(entry->index) +
					                                " has clocked animation frames; playback is not implemented");
			}
			bool cockpit = false;
			for (const auto &bitmap : cockpit_bitmap) cockpit = cockpit || bitmap.index == entry->index;
			for (int gauge = 0; gauge < MAX_GAUGE_BMS; ++gauge)
				cockpit = cockpit || Gauges[gauge].index == entry->index || Gauges_hires[gauge].index == entry->index;
			if (cockpit) throw std::runtime_error(pog_name + ": RGBA bitmap " + std::to_string(entry->index) +
				                                  " replaces a cockpit/HUD image; true-color cockpit layout is not implemented");
			if (const char *reason = ogl_xl_rgba_prepare(int(entry->width), int(entry->frame_height)))
				throw std::runtime_error(pog_name + ": " + reason);
		}
	}
	if (level && std::any_of(level->objects.begin(), level->objects.end(), [](const xl_compat::Object &object) { return object.type == 16; }))
		prepare_camera_model();
}

void XlPreparedResources::prepare_camera_model()
{
	xl_camera_validate_weapon_definitions();
	if (state->camera_model >= 0) return;
	if (state->installed) throw std::runtime_error("Camera resources must be prepared before installation");
	CameraAddonFile addon;
	if (!bm_validate_level_hxm(addon.file.get())) throw std::runtime_error("Camera addon HXM is malformed or uses unqualified serialized indices");
	// Prepare transactionally; a rejected addon leaves this owner's prior definitions intact
	auto staged = std::make_unique<State>(state->definitions);
	staged->graph = state->graph;
	const auto ids = validate_hxm_dependencies(addon.file.get(), staged->definitions, staged->graph, staged->changes, true);
	// Validate the camera append independently of earlier level model replacements
	// Those robots are checked by the level walk with their authored activation mode
	staged->changes.robots.insert(state->changes.robots.begin(), state->changes.robots.end());
	staged->changes.joints.insert(state->changes.joints.begin(), state->changes.joints.end());
	staged->changes.models.insert(state->changes.models.begin(), state->changes.models.end());
	staged->changes.bitmaps.insert(state->changes.bitmaps.begin(), state->changes.bitmaps.end());
	staged->changes.pointers.insert(state->changes.pointers.begin(), state->changes.pointers.end());
	staged->prepare_native_models();
	state->definitions = std::move(staged->definitions);
	state->changes = std::move(staged->changes);
	state->graph = std::move(staged->graph);
	state->native_models = std::move(staged->native_models);
	state->camera_robot = ids.first;
	state->camera_model = ids.second;
	state->has_hxm = true;
}

int XlPreparedResources::camera_robot_id() const
{
	return state->camera_robot;
}
int XlPreparedResources::camera_model_id() const
{
	return state->camera_model;
}

void XlPreparedResources::begin_hitbox_frame()
{
	if (state->hitbox_frame == INT32_MAX) {
		state->hitboxes.clear();
		state->hitbox_frame = 1;
	} else ++state->hitbox_frame;
}
int XlPreparedResources::camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, vms_vector &hit, vms_vector &normal)
{
	if (installed_resources != this) throw std::runtime_error("Camera query resource owner is no longer installed");
	return camera_hitbox_query(camera, moving, start, end, state->hitbox_frame, hit, normal);
}
extern "C" void xl_hitbox_begin_frame(void)
{
	if (installed_resources) installed_resources->begin_hitbox_frame();
}
extern "C" int xl_camera_hitbox_query(const object *camera, const object *moving, const vms_vector *start, const vms_vector *end, vms_vector *hit, vms_vector *normal)
{
	try {
		if (!xl_level_extensions_active() || !installed_resources || !camera || !moving || !start || !end || !hit || !normal)
			throw std::runtime_error("Camera query has no active native resource context");
		return installed_resources->camera_hitbox_query(*camera, *moving, *start, *end, *hit, *normal);
	} catch (const std::exception &error) {
		Error("Cannot query XL camera geometry: %s", error.what());
	}
	return 0;
}
void XlPreparedResources::validate_camera_query(const object &camera, const object &moving) const
{
	if (!state->installed || camera.type != 16 || camera.control_type != 18 || camera.movement_type != MT_NONE ||
	    camera.id != state->camera_robot || camera.render_type != RT_POLYOBJ || camera.rtype.pobj_info.model_num != state->camera_model ||
	    (camera.ctype.ai_info.behavior != 128 && camera.ctype.ai_info.behavior != 129 && camera.ctype.ai_info.behavior != 135))
		throw std::runtime_error("Camera query exceeds installed camera profile");
	if (moving.type != OBJ_PLAYER && moving.type != OBJ_WEAPON && moving.type != OBJ_ROBOT && moving.type != OBJ_FIREBALL) throw std::runtime_error("Camera query actor is not qualified");
}
int XlPreparedResources::camera_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int radius, vms_vector &hit, vms_vector &normal)
{
	if (installed_resources != this) throw std::runtime_error("Camera query resource owner is no longer installed");
	validate_camera_query(camera, moving);
	if (Game_mode & GM_MULTI) throw std::runtime_error("Camera query requires the single-player source profile");
	// Default player profile nHitboxes=2 makes source UseHitbox false for every actor
	// The camera RegisterHit returns unchanged for OBJ_CAMBOT, without hit-state writes
	const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
	xl_compat::FixedLightPoint intersection{};
	const int size = radius < 0 ? 0 : camera.size;
	const auto result = xl_compat::model_sphere_query(intersection, point(start), point(end), point(camera.pos), xl_compat::hitbox_fixed(double(size) + radius));
	if (result < 0) return 0;
	hit = { intersection[0], intersection[1], intersection[2] };
	normal = {};
	return result;
}
extern "C" int xl_camera_query(const object *camera, const object *moving, const vms_vector *start, const vms_vector *end, int radius, vms_vector *hit, vms_vector *normal)
{
	try {
		if (!xl_level_extensions_active() || !installed_resources || !camera || !moving || !start || !end || !hit || !normal)
			throw std::runtime_error("Camera query has no active native resource context");
		return installed_resources->camera_query(*camera, *moving, *start, *end, radius, *hit, *normal);
	} catch (const std::exception &error) {
		Error("Cannot query XL camera geometry: %s", error.what());
	}
	return 0;
}
int XlPreparedResources::camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int frame, vms_vector &hit, vms_vector &normal)
{
	validate_camera_query(camera, moving);
	if (moving.type != OBJ_PLAYER && moving.type != OBJ_WEAPON) throw std::runtime_error("Forced camera hitbox profile requires a player or weapon");
	if (moving.type == OBJ_WEAPON && (moving.id == PROXIMITY_ID || moving.id == SUPERPROX_ID || moving.id == PMINE_ID || moving.id == ROBOT_SUPERPROX_ID)) throw std::runtime_error("Camera mine query requires source sphere routing");
	const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
	const auto matrix = [&](const vms_matrix &m) { return std::array<xl_compat::FixedLightPoint, 3>{ point(m.rvec), point(m.uvec), point(m.fvec) }; };
	const auto radius = [&](const object &actor) {
		const int model = actor.rtype.pobj_info.model_num;
		if (actor.render_type != RT_POLYOBJ || model < 0) return actor.size;
		if (std::size_t(model) >= state->definitions.models.size()) throw std::runtime_error("Camera query model is missing");
		return model_collision_radius(model);
	};
	const auto p0 = point(start), p1 = point(end), position = point(camera.pos);
	const bool polygon = moving.render_type == RT_POLYOBJ && moving.rtype.pobj_info.model_num >= 0 &&
	                     !(moving.type == OBJ_WEAPON && (moving.id == GAUSS_ID || moving.id == VULCAN_ID));
	xl_compat::FixedLightPoint intersection{}, surface_normal{};
	int16_t model = -1;
	const xl_compat::HitboxQueryActor target{ &model_hitboxes(state->camera_model), matrix(camera.orient), position, camera.size, radius(camera), model_sphere(state->camera_model).center };
	const xl_compat::HitboxQueryActor actor{ polygon ? &model_hitboxes(moving.rtype.pobj_info.model_num) : nullptr, matrix(moving.orient), point(moving.pos), moving.size, radius(moving), polygon ? model_sphere(moving.rtype.pobj_info.model_num).center : xl_compat::FixedLightPoint{} };
	const auto result = xl_compat::hitbox_object_query(intersection, surface_normal, model, target, actor, frame, 1, p0, p1);
	if (result) {
		hit = { intersection[0], intersection[1], intersection[2] };
		normal = { surface_normal[0], surface_normal[1], surface_normal[2] };
	}
	return result;
}

xl_compat::ModelHitboxTransform &XlPreparedResources::model_hitboxes(int model)
{
	if (!state->installed || model < 0 || std::size_t(model) >= state->definitions.models.size())
		throw std::runtime_error("Hitbox model is not installed in this level");
	state->hitboxes.resize(state->definitions.models.size());
	auto &cached = state->hitboxes[model];
	if (!cached) {
		const auto &header = state->definitions.models[model];
		const auto &bytes = state->definitions.payloads[model];
		if (bytes.empty() || bytes.size() != std::size_t(header.model_data_size))
			throw std::runtime_error("Hitbox model payload is missing or truncated");
		cached = std::make_unique<State::CollisionModel>(xl_compat::ModelRenderGeometry(bytes.data(), bytes.size()));
	}
	return cached->transform;
}
const xl_compat::RenderModelSphere &XlPreparedResources::model_sphere(int model)
{
	model_hitboxes(model);
	return state->hitboxes[model]->sphere;
}
int XlPreparedResources::model_collision_radius(int model)
{
	const auto &sphere = model_sphere(model);
	const int base = state->definitions.base_radii.at(model);
	return base ? base : sphere.radius;
}

void XlPreparedResources::initialize_camera_objects()
{
	if (state->camera_model < 0) return;
	if (!state->installed) throw std::runtime_error("Camera objects require installed addon definitions");
	static_assert(IMMORTAL_TIME == 0x3fffffff && MT_NONE == 0 && AIM_STILL == 0, "Camera source constants differ from native runtime");
	// Validate every old model before changing any object
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if (object.type == 16 && object.render_type == RT_POLYOBJ &&
		    (object.rtype.pobj_info.model_num < 0 || object.rtype.pobj_info.model_num >= N_polygon_models))
			throw std::runtime_error("Camera object references an unavailable pre-initialization model");
	}
	for (int i = 0; i <= Highest_object_index; ++i) {
		auto &object = Objects[i];
		if (object.type != 16) continue; // OBJ_CAMBOT, distinct from transient OBJ_CAMERA 6
		const int radius = object.render_type == RT_POLYOBJ ? Polygon_models[object.rtype.pobj_info.model_num].rad : object.size;
		xl_compat::CameraObjectState normalized{ object.id, object.control_type, object.movement_type, object.rtype.pobj_info.model_num, Ai_local_info[i].mode, object.size, object.lifeleft };
		xl_compat::initialize_camera_object(normalized, state->camera_robot, state->camera_model, radius);
		object.id = normalized.id;
		object.control_type = normalized.control;
		object.movement_type = normalized.movement;
		object.size = normalized.size;
		object.lifeleft = normalized.life;
		object.rtype.pobj_info.model_num = normalized.model;
		Ai_local_info[i].mode = normalized.ai_mode;
	}
}

void XlPreparedResources::install_pog()
{
	if (!xl_pog_install(state->pog.get())) throw std::runtime_error("cannot allocate prepared POG bitmap storage");
}

extern "C" void xl_initialize_camera_objects(void)
{
	if (!prepared_resources) return;
	try {
		prepared_resources->initialize_camera_objects();
	} catch (const std::exception &error) {
		Error("Cannot initialize prepared XL cameras: %s", error.what());
	}
}

void XlPreparedResources::install_messages()
{
	xl_compat::install_messages(state->messages);
	xl_compat::install_named_audio(state->audio);
}

const xl_compat::ParticleImageResource *XlPreparedResources::particle_image(int subtype) const
{
	return subtype == 3 ? state->particle_image.get() : subtype == 2 ? state->bubble_image.get()
	                                                : subtype == 0   ? state->smoke_image.get()
	                                                : subtype == 5   ? state->rain_image.get()
	                                                : subtype == 6   ? state->snow_image.get()
	                                                                 : nullptr;
}

const xl_compat::ParticleImageResource *xl_prepared_particle_image(int subtype)
{
	return prepared_resources ? prepared_resources->particle_image(subtype) : nullptr;
}

const xl_compat::ParticleImageResource *XlPreparedResources::lightning_image() const
{
	return state->lightning_image.get();
}

const xl_compat::ParticleImageResource *xl_prepared_lightning_image()
{
	return prepared_resources ? prepared_resources->lightning_image() : nullptr;
}

const xl_compat::ParticleImageResource *XlPreparedResources::powerup_glare_image() const
{
	return state->powerup_glare_image.get();
}

const xl_compat::ParticleImageResource *xl_prepared_powerup_glare_image()
{
	return prepared_resources ? prepared_resources->powerup_glare_image() : nullptr;
}
const xl_compat::AnimatedImageResource *XlPreparedResources::time_equipment_image(unsigned index) const
{
	return state->time_equipment_images.at(index).get();
}
const xl_compat::AnimatedImageResource *xl_prepared_time_equipment_image(unsigned index)
{
	return prepared_resources ? prepared_resources->time_equipment_image(index) : nullptr;
}

bool XlPreparedResources::texture_brightness(int texture, int &brightness) const
{
	if (!state->lighting.brightness || texture < 0 || std::size_t(texture) >= state->lighting.brightness->size()) return false;
	brightness = (*state->lighting.brightness)[texture];
	return true;
}

extern "C" int xl_prepared_texture_brightness(int texture, int *brightness)
{
	return brightness && prepared_resources && prepared_resources->texture_brightness(texture, *brightness);
}

bool XlPreparedResources::pog_receipt(int bitmap, unsigned *bytes, unsigned *fnv1a) const
{
	return xl_pog_bitmap_receipt(state->pog.get(), bitmap, bytes, fnv1a) != 0;
}

extern "C" int xl_prepared_pog_receipt(int bitmap, unsigned *bytes, unsigned *fnv1a)
{
	return prepared_resources && prepared_resources->pog_receipt(bitmap, bytes, fnv1a);
}

void XlPreparedResources::install_hxm()
{
	// Allocate publication copies before changing any live definitions
	auto models = state->owned_models.models;
	auto sources = state->owned_models.sources;
	if (!state->has_hxm) {
		xl_oof_renderer_clear();
		mission_oof_models = std::move(models);
		mission_mod_sources = std::move(sources);
		state->installed = true;
		installed_resources = this;
		return;
	}
	state->hitboxes.clear();
	// A repeated native reload gets fresh buffers before any live definitions change
	for (const int id : state->changes.models)
		if (!state->native_models[id]->model.model_data) {
			state->prepare_native_models();
			break;
		}
	auto &definitions = state->definitions;
	for (const int id : state->changes.robots) Robot_info[id] = definitions.robots[id];
	for (const int id : state->changes.joints) Robot_joints[id] = definitions.joints[id];
	for (const int id : state->changes.bitmaps) ObjBitmaps[id] = definitions.bitmaps[id];
	for (const int id : state->changes.pointers) ObjBitmapPtrs[id] = definitions.pointers[id];
	for (const int id : state->changes.models) {
		free_model(&Polygon_models[id]);
		Polygon_models[id] = state->native_models[id]->model;
		Dying_modelnums[id] = definitions.dying[id];
		Dead_modelnums[id] = definitions.dead[id];
		state->native_models[id]->model.model_data = nullptr;
	}
	N_robot_types = int(definitions.robots.size());
	N_robot_joints = int(definitions.joints.size());
	N_polygon_models = int(definitions.models.size());
	Robot_replacements_loaded = 1;
	xl_oof_renderer_clear();
	mission_oof_models = std::move(models);
	mission_mod_sources = std::move(sources);
	state->installed = true;
	state->hitbox_frame = 1;
	installed_resources = this;
}

int XlPreparedResources::custom_model_radius(int model, int fallback) const
{
	if (!state->installed || model < 0 || std::size_t(model) >= state->definitions.custom_radii.size()) return fallback;
	const auto radius = state->definitions.custom_radii[model];
	return radius ? radius : fallback;
}

extern "C" int xl_custom_model_radius(int model, int fallback)
{
	if (!xl_level_extensions_active() || !prepared_resources) return fallback;
	return prepared_resources->custom_model_radius(model, fallback);
}

void xl_set_prepared_resources(const char *level_filename, std::unique_ptr<XlPreparedResources> resources)
{
	prepared_level = level_filename;
	prepared_resources = std::move(resources);
	prepared_resources->install_messages();
}

extern "C" void xl_discard_prepared_resources(void)
{
	prepared_resources.reset();
	prepared_level.clear();
}

extern "C" int xl_install_prepared_hxm(const char *level_filename)
{
	if (!prepared_resources || d_stricmp(prepared_level.c_str(), level_filename)) return 0;
	try {
		prepared_resources->install_hxm();
	} catch (const std::exception &error) {
		Error("Cannot install prepared XL definitions: %s", error.what());
	}
	return 1;
}

extern "C" int xl_install_prepared_pog(const char *level_filename)
{
	if (!prepared_resources || d_stricmp(prepared_level.c_str(), level_filename)) return 0;
	try {
		prepared_resources->install_pog();
	} catch (const std::exception &error) {
		Error("Cannot install prepared XL textures: %s", error.what());
	}
	return 1;
}
