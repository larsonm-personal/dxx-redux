#include "resource_validation.h"
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
#include "picosha2.h"
#include "asset_dependencies.h"
#include "pog_resources.h"
#include "pog_renderer.h"
#include "named_audio.h"
#include "companion_resources.h"
#include "../inventory.h"
#include "preview.h"
#include "particle_resources.h"
#include <utility>
extern "C" {
#include "inferno.h"
#include "bm.h"
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
extern int Robot_replacements_loaded;
}

namespace
{
static_assert(MAX_SUBMODELS == 10, "Update the bounded XL model geometry capacity with native submodels");
using File = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
std::string startup_ham_name, startup_ham_hash, startup_origin, base_rejection;
const std::array<const char *, 2> sound_banks{ "descent2.s11", "descent2.s22" };
std::array<std::string, 2> startup_sound_hashes;
std::map<std::string, std::string> startup_graphics_hashes;
bool mission_requires_xl = false;

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

std::pair<int, int> validate_hxm_dependencies(PHYSFS_file *file, MissionDefinitions &definitions, xl_assets::ObjectDependencies &graph, HxmChanges &changes, bool camera = false)
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
		std::fill(std::begin(robot.turn_time), std::end(robot.turn_time), 0);
		std::fill(std::begin(robot.max_speed), std::end(robot.max_speed), 0);
		std::fill(std::begin(robot.circle_distance), std::end(robot.circle_distance), 0);
		std::fill(std::begin(robot.rapidfire_count), std::end(robot.rapidfire_count), 0);
		if (models[camera_ids.second].n_models != 1 || models[camera_ids.second].simpler_model ||
		    dying[camera_ids.second] != -1 || dead[camera_ids.second] != -1)
			throw std::runtime_error("Camera addon profile: guns=" + std::to_string(robot.n_guns) + ", submodels=" + std::to_string(models[camera_ids.second].n_models) +
			                         ", simpler=" + std::to_string(models[camera_ids.second].simpler_model) + ", dying=" + std::to_string(dying[camera_ids.second]) + ", dead=" + std::to_string(dead[camera_ids.second]));
	}
	for (const int id : changed_models) graph.model(id);
	for (const int id : changed_bitmaps) graph.bitmap(bitmaps[id].index);
	for (const int id : changed_pointers) {
		if (pointers[id] >= N_ObjBitmaps) throw std::runtime_error("HXM model texture references missing object bitmap");
		graph.bitmap(bitmaps[pointers[id]].index);
	}
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
		if (!mission_owns_resource_directory(origin) && !mission_owns_resource_archive(origin) && !xl_resource_owns_installation(origin))
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
				if (!owned(path)) throw std::runtime_error("Missing owned cambot.hxm or data/d2x-xl.hog camera addon");
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
	mission_requires_xl = requires_xl != 0;
	base_rejection.clear();
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
	return source && !startup_origin.empty() && startup_origin == source;
}

extern "C" const char *xl_resource_base_rejection(void)
{
	base_rejection.clear();
	try {
		// The browser flag is only a hint; mounted level headers remain authoritative
		if (!mission_requires_xl && !mounted_mission_requires_xl()) return base_rejection.c_str();
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
	std::shared_ptr<const xl_compat::ParticleImageResource> lightning_image;
	std::shared_ptr<const xl_compat::ParticleImageResource> powerup_glare_image;
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

	explicit State(const MissionDefinitions &base) : definitions(base)
	{
		// Vector copying owns bytes, but model headers still need their pointers rebound
		for (std::size_t id = 0; id < definitions.models.size(); ++id)
			if (!definitions.payloads[id].empty())
				definitions.models[id].model_data = definitions.payloads[id].data();
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
	if (level) state->messages = xl_compat::prepare_messages(level_filename, *level);
	if (level) state->audio = xl_compat::prepare_named_audio(level_filename, *level, ordinal);
	if (level) {
		state->particle_image = xl_compat::prepare_particle_image(*level);
		state->smoke_image = xl_compat::prepare_particle_image(*level, 0);
		state->bubble_image = xl_compat::prepare_particle_image(*level, 2);
		state->lightning_image = xl_compat::prepare_lightning_image(*level);
		state->powerup_glare_image = xl_compat::prepare_powerup_glare_image();
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
		if (!bm_validate_level_hxm(file.get()))
			throw std::runtime_error("HXM definitions are malformed or require unsupported normalization: " + hxm_name);
		try {
			validate_hxm_dependencies(file.get(), state->definitions, state->graph, state->changes);
			state->has_hxm = true;
			state->prepare_native_models();
		} catch (const std::exception &error) {
			throw std::runtime_error("HXM dependencies: " + hxm_name + ": " + error.what());
		}
	}
	const auto pog_name = base + ".POG";
	if (auto file = companion(pog_name)) {
		char error[256]{};
		state->pog.reset(xl_pog_prepare(file.get(), Num_bitmap_files, error, sizeof(error)));
		if (!state->pog) throw std::runtime_error(pog_name + ": " + error);
		for (int i = 0; i < xl_pog_count(state->pog.get()); ++i) {
			const auto *entry = xl_pog_entry(state->pog.get(), i);
			if (!entry->rgba) continue;
			bool cockpit = false;
			for (const auto &bitmap : cockpit_bitmap) cockpit = cockpit || bitmap.index == entry->index;
			for (int gauge = 0; gauge < MAX_GAUGE_BMS; ++gauge)
				cockpit = cockpit || Gauges[gauge].index == entry->index || Gauges_hires[gauge].index == entry->index;
			if (cockpit) throw std::runtime_error(pog_name + ": RGBA bitmap " + std::to_string(entry->index) +
				                                  " replaces a cockpit/HUD image; true-color cockpit layout is not implemented");
			if (const char *reason = ogl_xl_rgba_prepare(int(entry->width), int(entry->height)))
				throw std::runtime_error(pog_name + ": " + reason);
		}
	}
	if (level && std::any_of(level->objects.begin(), level->objects.end(), [](const xl_compat::Object &object) { return object.type == 16; }))
		prepare_camera_model();
}

void XlPreparedResources::prepare_camera_model()
{
	if (state->camera_model >= 0) return;
	if (state->installed) throw std::runtime_error("Camera resources must be prepared before installation");
	CameraAddonFile addon;
	if (!bm_validate_level_hxm(addon.file.get())) throw std::runtime_error("Camera addon HXM is malformed or uses unqualified serialized indices");
	// Prepare transactionally; a rejected addon leaves this owner's prior definitions intact
	auto staged = std::make_unique<State>(state->definitions);
	staged->changes = state->changes;
	staged->graph = state->graph;
	const auto ids = validate_hxm_dependencies(addon.file.get(), staged->definitions, staged->graph, staged->changes, true);
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
	if (moving.type != OBJ_PLAYER && moving.type != OBJ_WEAPON) throw std::runtime_error("Camera query actor is not qualified");
}
int XlPreparedResources::camera_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int radius, vms_vector &hit, vms_vector &normal)
{
	if (installed_resources != this) throw std::runtime_error("Camera query resource owner is no longer installed");
	validate_camera_query(camera, moving);
	if (Game_mode & GM_MULTI) throw std::runtime_error("Camera query requires the single-player source profile");
	// Default player profile nHitboxes=2 makes source UseHitbox false for both actors
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
	if (!state->has_hxm) return;
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
