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
#include <cstdlib>
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
#include "prepared_ham.h"
#include "retained_resource.h"
#include "target_sources.h"
#include "level_requirements.h"
#include "mission_resource_scope.h"
#ifdef __ANDROID__
#include "../../app/src/main/cpp/shared/android_mission_assets.h"
#include "../../app/src/main/cpp/shared/android_sound_trace.h"
#endif
#if defined(__ANDROID__) || defined(DXX_XL_CHECKPOINT_EXPORT)
#include "../../app/src/main/cpp/shared/mod_save_compat.h"
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
char startup_model_names[MAX_POLYGON_MODELS][FILENAME_LEN]{};
std::string vertigo_rejection, mod_rejection;
const std::array<const char *, 2> sound_banks{ "descent2.s11", "descent2.s22" };
std::array<std::string, 2> startup_sound_hashes;
std::map<std::string, std::string> startup_graphics_hashes;
struct TrustedBankSource {
	std::string origin, member, hash;
};
std::map<std::string, TrustedBankSource> startup_bank_sources;
// Retain only the latest immutable pair; individual candidates own their references
std::shared_ptr<const XlResourceBanks> prepared_bank_cache;
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
	if (prepared_bank_cache)
		if (const auto *hash = prepared_bank_cache->verified_hash(bytes)) return *hash;
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

std::pair<std::string, std::string> graphics_bank_names(const std::string &palette)
{
	if (palette.empty() || palette.size() >= FILENAME_LEN || palette.find_first_of("/\\:") != std::string::npos)
		throw std::runtime_error("unsupported level palette name: " + palette);
	const auto name = resource_key(palette);
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos || name.substr(dot) != ".256")
		throw std::runtime_error("unsupported level palette extension: " + palette);
	return { name, name.substr(0, dot) + ".pig" };
}

void capture_bank_origin(const std::string &name, const std::string &hash)
{
	if (hash.empty()) return;
	std::string actual = name;
	if (PHYSFSEXT_locateCorrectCase(actual.data()) != 0) throw std::runtime_error("Cannot retain startup bank member identity");
	const char *origin = PHYSFS_getRealDir(actual.c_str());
	if (!origin || actual.find_first_of("/\\:") != std::string::npos) throw std::runtime_error("Cannot retain flat startup bank origin");
	startup_bank_sources[resource_key(name)] = { origin, actual, hash };
}

std::vector<unsigned char> retained_bank_source(const std::string &name)
{
	const auto found = startup_bank_sources.find(name);
	if (found == startup_bank_sources.end()) throw std::runtime_error("No qualified startup bank origin: " + name);
	auto bytes = xl_retain_origin_resource(found->second.origin, found->second.member);
	if (picosha2::hash256_hex_string(bytes) != found->second.hash)
		throw std::runtime_error("Qualified startup bank changed: " + name);
	return bytes;
}

struct MissionDefinitions {
	std::vector<robot_info> robots{ Robot_info, Robot_info + N_robot_types };
	std::vector<jointpos> joints{ Robot_joints, Robot_joints + N_robot_joints };
	std::vector<polymodel> models{ Polygon_models, Polygon_models + N_polygon_models };
	std::vector<int> dying{ Dying_modelnums, Dying_modelnums + N_polygon_models };
	std::vector<int> dead{ Dead_modelnums, Dead_modelnums + N_polygon_models };
	std::vector<bitmap_index> bitmaps{ ObjBitmaps, ObjBitmaps + MAX_OBJ_BITMAPS };
	std::vector<ushort> pointers{ ObjBitmapPtrs, ObjBitmapPtrs + MAX_OBJ_BITMAPS };
	std::vector<weapon_info> weapons{ Weapon_info, Weapon_info + N_weapon_types };
	std::vector<powerup_type_info> powerups{ Powerup_info, Powerup_info + N_powerup_types };
	std::vector<vclip> clips{ Vclip, Vclip + Num_vclips };
	std::vector<eclip> effects{ Effects, Effects + Num_effects };
	std::vector<wclip> wall_clips{ WallAnims, WallAnims + Num_wall_anims };
	std::vector<bitmap_index> textures{ Textures, Textures + NumTextures };
	std::vector<tmap_info> texture_info{ TmapInfo, TmapInfo + NumTextures };
	std::array<ubyte, MAX_SOUNDS> sounds, alternate_sounds;
	std::array<bitmap_index, N_COCKPIT_BITMAPS> cockpits{};
	std::array<bitmap_index, MAX_GAUGE_BMS> gauges{}, gauges_hires{};
	int object_bitmap_count = N_ObjBitmaps;
	int low_memory = GameArg.SysLowMem;
	int player_model = Player_ship->model_num, reactor_count = Num_reactors;
	char model_names[MAX_POLYGON_MODELS][FILENAME_LEN]{};
	std::vector<std::vector<ubyte>> payloads;
	std::vector<int> custom_radii;
	std::vector<int> base_radii;

	MissionDefinitions() : payloads(models.size()), custom_radii(models.size()), base_radii(models.size())
	{
		std::memcpy(model_names, Pof_names, sizeof(model_names));
		std::copy_n(cockpit_bitmap, cockpits.size(), cockpits.begin());
		std::copy_n(Gauges, gauges.size(), gauges.begin());
		std::copy_n(Gauges_hires, gauges_hires.size(), gauges_hires.begin());
		std::copy_n(Sounds, MAX_SOUNDS, sounds.begin());
		std::copy_n(AltSounds, MAX_SOUNDS, alternate_sounds.begin());
		for (std::size_t id = 0; id < models.size(); ++id) {
			auto &model = models[id];
			base_radii[id] = model.rad;
			if (model.model_data && model.model_data_size > 0) {
				payloads[id].assign(model.model_data, model.model_data + model.model_data_size);
				model.model_data = payloads[id].data();
			}
		}
	}
	explicit MissionDefinitions(const bm_ham_destination &target) : robots(target.Robot_info, target.Robot_info + *target.N_robot_types),
	                                                                joints(target.Robot_joints, target.Robot_joints + *target.N_robot_joints),
	                                                                models(target.Polygon_models, target.Polygon_models + *target.N_polygon_models),
	                                                                dying(target.Dying_modelnums, target.Dying_modelnums + *target.N_polygon_models),
	                                                                dead(target.Dead_modelnums, target.Dead_modelnums + *target.N_polygon_models),
	                                                                bitmaps(target.ObjBitmaps, target.ObjBitmaps + MAX_OBJ_BITMAPS),
	                                                                pointers(target.ObjBitmapPtrs, target.ObjBitmapPtrs + MAX_OBJ_BITMAPS),
	                                                                weapons(target.Weapon_info, target.Weapon_info + *target.N_weapon_types),
	                                                                powerups(target.Powerup_info, target.Powerup_info + *target.N_powerup_types),
	                                                                clips(target.Vclip, target.Vclip + *target.Num_vclips),
	                                                                effects(target.Effects, target.Effects + *target.Num_effects),
	                                                                wall_clips(target.WallAnims, target.WallAnims + *target.Num_wall_anims),
	                                                                textures(target.Textures, target.Textures + *target.NumTextures),
	                                                                texture_info(target.TmapInfo, target.TmapInfo + *target.NumTextures),
	                                                                object_bitmap_count(*target.N_ObjBitmaps),
	                                                                payloads(models.size()), custom_radii(models.size()), base_radii(models.size())
	{
		std::copy_n(target.Sounds, MAX_SOUNDS, sounds.begin());
		player_model = target.only_player_ship->model_num;
		reactor_count = *target.Num_reactors;
		std::memcpy(model_names, startup_model_names, sizeof(model_names));
		std::copy_n(target.cockpit_bitmap, cockpits.size(), cockpits.begin());
		std::copy_n(target.Gauges, gauges.size(), gauges.begin());
		std::copy_n(target.Gauges_hires, gauges_hires.size(), gauges_hires.begin());
		std::copy_n(target.AltSounds, MAX_SOUNDS, alternate_sounds.begin());
		for (std::size_t id = 0; id < models.size(); ++id) {
			auto &model = models[id];
			base_radii[id] = model.rad;
			if (model.model_data && model.model_data_size > 0) {
				payloads[id].assign(model.model_data, model.model_data + model.model_data_size);
				model.model_data = payloads[id].data();
			}
		}
	}
	void bind_dependencies(xl_assets::ObjectDependencies &graph) const
	{
		graph.player_model = player_model;
		graph.reactor_count = reactor_count;
		graph.model_names = model_names;
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
		graph.weapon_data = weapons.data();
		graph.powerup_data = powerups.data();
		graph.clip_data = clips.data();
		graph.effect_data = effects.data();
		graph.wall_clip_data = wall_clips.data();
		graph.texture_data = textures.data();
		graph.texture_info = texture_info.data();
		graph.weapon_count = int(weapons.size());
		graph.powerup_count = int(powerups.size());
		graph.clip_count = int(clips.size());
		graph.effect_count = int(effects.size());
		graph.wall_clip_count = int(wall_clips.size());
		graph.texture_count = int(textures.size());
		graph.object_bitmap_count = object_bitmap_count;
		graph.sound_mappings = sounds.data();
		graph.alternate_sounds = alternate_sounds.data();
		graph.low_memory = low_memory;
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
		polygon_model_data_init_scoped(&model);
	}
	~NativeModel()
	{
		if (model.model_data) free_model(&model);
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
	std::set<int> robots, joints, models, bitmaps, pointers, weapons;
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
	definitions.bind_dependencies(graph);
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
		if (pointers[id] >= graph.object_bitmap_count) throw std::runtime_error("HXM model texture references missing object bitmap");
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
		std::memcpy(startup_model_names, Pof_names, sizeof(startup_model_names));
		auto corrected = startup_ham_name;
		const char *origin = PHYSFSEXT_locateCorrectCase(corrected.data()) == 0 ? PHYSFS_getRealDir(corrected.c_str()) : nullptr;
		startup_origin = origin ? origin : "";
		if (origin) startup_ham_name = corrected;
		startup_bank_sources.clear();
		// Both banks matter: playback may fall back and authored durations use the canonical bank
		for (std::size_t i = 0; i < sound_banks.size(); ++i) {
			startup_sound_hashes[i] = optional_resource_hash(sound_banks[i]);
			capture_bank_origin(sound_banks[i], startup_sound_hashes[i]);
		}
		startup_graphics_hashes.clear();
		std::unique_ptr<char *, decltype(&PHYSFS_freeList)> files(PHYSFS_enumerateFiles(""), PHYSFS_freeList);
		if (!files) throw std::runtime_error("cannot enumerate startup graphics resources");
		for (char **entry = files.get(); *entry; ++entry) {
			const auto key = resource_key(*entry);
			const auto dot = key.find_last_of('.');
			if (dot != std::string::npos && (key.substr(dot) == ".256" || key.substr(dot) == ".pig")) {
				const auto hash = optional_resource_hash(*entry);
				startup_graphics_hashes.emplace(key, hash);
				capture_bank_origin(*entry, hash);
			}
		}
	} catch (...) {
		startup_ham_hash.clear();
		startup_ham_name.clear();
		startup_origin.clear();
		startup_sound_hashes = {};
		startup_graphics_hashes.clear();
		startup_bank_sources.clear();
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
struct IncomingSourceFile {
	struct DirectoryLease {
		xl_mission_resource_scope scope{};
		std::string path;
		~DirectoryLease()
		{
			xl_mission_resource_release(&scope, path.c_str());
		}
	};
	// Declaration order closes the member before releasing its private mount
	std::unique_ptr<DirectoryLease> lease;
	File file{ nullptr, PHYSFS_close };
	PHYSFS_file *get() const
	{
		return file.get();
	}
	explicit operator bool() const
	{
		return bool(file);
	}
};

std::string incoming_mod_directory(mission_preparation *target, const std::string &name)
{
	std::filesystem::path directory(mission_prepared_directory(target));
	// Resolve only the two owned directory components; member case lookup inside
	// that subtree remains the existing PhysicsFS owner's job
	for (const auto &component : { std::string("mods"), name }) {
		if (!std::filesystem::is_directory(directory)) return {};
		std::filesystem::path selected;
		for (const auto &entry : std::filesystem::directory_iterator(directory)) {
			if (d_stricmp(entry.path().filename().string().c_str(), component.c_str()) || !entry.is_directory()) continue;
			if (!selected.empty()) throw std::runtime_error("Ambiguous incoming mod directory case");
			selected = entry.path();
		}
		if (selected.empty()) return {};
		directory = std::move(selected);
	}
	return directory.string();
}

IncomingSourceFile incoming_owned_file(mission_preparation *target, const std::string &path)
{
	IncomingSourceFile result;
	if (!target) {
		result.file.reset(PHYSFSX_openReadBuffered(path.c_str()));
		return result;
	}
	// Mount the target's mod subtree privately even when another mission shadows
	// the same logical basename. Never rearrange the outgoing search path
	const auto separator = path.find('/', 5);
	if (path.compare(0, 5, "mods/") || separator == std::string::npos)
		throw std::runtime_error("Invalid incoming mod resource path");
	const auto directory = incoming_mod_directory(target, path.substr(5, separator - 5));
	if (!directory.empty()) {
		result.lease = std::make_unique<IncomingSourceFile::DirectoryLease>();
		result.lease->path = directory;
		if (PHYSFS_getMountPoint(directory.c_str()))
			throw std::runtime_error("Incoming mod directory is already leased");
		result.file.reset(xl_mission_resource_open(&result.lease->scope, result.lease->path.c_str(), nullptr,
		                                           path.substr(separator + 1).c_str()));
		if (result.file) return result;
		result.lease.reset();
	}
	result.file.reset(mission_prepared_open(target, path.c_str()));
	if (result.file) return result;
	return result;
}

std::string owned_mod_path(const char *extension, const std::string &relative, mission_preparation *target)
{
	const std::string archive = target ? mission_prepared_archive(target) : mission_resource_archive();
	const auto slash = archive.find_last_of("/\\");
	const auto leaf = archive.substr(slash == std::string::npos ? 0 : slash + 1);
	const auto dot = leaf.find_last_of('.');
	if (dot == std::string::npos || resource_key(leaf.substr(dot)) != ".hog") return {};
	const auto name = leaf.substr(0, dot);
	if (name.empty() || name == "." || name == ".." || name.find_first_of(":/\\") != std::string::npos)
		throw std::runtime_error("Invalid XL mission resource identity");
	return "mods/" + name + "/" + (relative.empty() ? name + extension : relative);
}

std::string owned_mod_file(const char *extension, const std::string &relative = {}, mission_preparation *target = nullptr)
{
	auto path = owned_mod_path(extension, relative, target);
	if (path.empty()) return {};
	if (target) return incoming_owned_file(target, path) ? path : std::string{};
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

std::vector<uint8_t> read_model_resource(const std::string &path, mission_preparation *target)
{
	auto file = incoming_owned_file(target, path);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length <= 0 || length > 64 * 1024 * 1024) throw std::runtime_error("Invalid owned model resource: " + path);
	std::vector<uint8_t> bytes(static_cast<std::size_t>(length));
	if (PHYSFS_readBytes(file.get(), bytes.data(), length) != length) throw std::runtime_error("Cannot read owned model resource: " + path);
	return bytes;
}

xl_compat::OwnedModelSet prepare_owned_models(int ordinal, mission_preparation *target = nullptr)
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
	// Check absent directories once per preparation instead of walking them for
	// every optional model name. Existing files still pass the owned-file checks
	if (!target
#ifdef __ANDROID__
	    && !*android_mission_assets_key()
#endif
	) {
		folders.erase(std::remove_if(folders.begin(), folders.end(), [](const std::string &folder) {
			              auto path = owned_mod_path("", folder.substr(0, folder.size() - 1), nullptr);
			              return path.empty() || PHYSFSEXT_locateCorrectCase(path.data()) != 0;
		              }),
		              folders.end());
	}
	if (folders.empty()) return result;
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
				path = owned_mod_file("", folder + candidate, target);
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
		const auto model_bytes = read_model_resource(path, target);
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
			auto skin_path = owned_mod_file("", selected_folder + texture, target);
			if (skin_path.empty() && selected_folder != "models/") skin_path = owned_mod_file("", "models/" + texture, target);
			if (skin_path.empty()) throw std::runtime_error("Missing owned OOF skin: " + texture);
			auto &skin = skins_by_path[skin_path];
			if (!skin) {
				const auto bytes = read_model_resource(skin_path, target);
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
		for (const auto &path : { vham }) {
			if (!ham.empty() || path.empty()) continue;
			File file(PHYSFSX_openReadBuffered(path.c_str()), PHYSFS_close);
			if (!file) throw std::runtime_error("Cannot read XL mission definition: " + path);
			sources[path] = resource_hash(file.get(), path);
		}
		if (!ham.empty()) {
			File file(PHYSFSX_openReadBuffered(ham.c_str()), PHYSFS_close);
			auto target = XlPreparedResources::prepare_mission(nullptr);
			target->install_hxm();
#if defined(__ANDROID__) || defined(DXX_XL_CHECKPOINT_EXPORT)
			mod_save_capture_definitions();
#endif
			input_demo_assets_extra_ham_loaded(ham.c_str(), file.get());
			if (!vham.empty()) {
#ifdef __ANDROID__
				android_sound_trace_asset_open("extra_ham", vham.c_str());
#endif
#if defined(__ANDROID__) || defined(DXX_XL_CHECKPOINT_EXPORT)
				mod_save_capture_extra_ham(vham.c_str());
#endif
				File extension(PHYSFSX_openReadBuffered(vham.c_str()), PHYSFS_close);
				input_demo_assets_extra_ham_loaded(vham.c_str(), extension.get());
			}
			con_printf(CON_NORMAL, "XL mission definitions: HAM=%s HXM=%s VHAM=%s robots=%d models=%d\n",
			           ham.c_str(), hxm.c_str(), vham.c_str(), N_robot_types, N_polygon_models);
			return 1;
		}
		if (ham.empty() && !hxm.empty()) {
			File file(PHYSFSX_openReadBuffered(hxm.c_str()), PHYSFS_close);
			sources[hxm] = resource_hash(file.get(), hxm);
			install_mission_hxm(file.get());
		}
		if (ham.empty() && !vham.empty() && !bm_read_xl_extra_robots(vham.c_str()))
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
	const auto names = graphics_bank_names(palette);
	const auto &name = names.first, &pig = names.second;
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
	struct Publication {
		xl_compat::OwnedModelSet owned_models;
		std::map<std::string, std::string> definition_sources;
		std::set<int> generated_models;
	};
	Publication publication;
	std::vector<unsigned char> level_bytes;
	std::unique_ptr<xl_compat::Inventory> level_inventory;
	std::shared_ptr<const XlResourceBanks> banks;
	std::vector<unsigned char> palette_bytes;
	std::unique_ptr<XlPreparedHam> incoming_ham;
	std::unique_ptr<XlPreparedHam> incoming_extra;
	std::map<std::string, std::string> definition_sources;
	std::set<int> generated_models;
	bool complete_mission = false;
	std::vector<std::vector<unsigned char>> definition_layers;
	xl_compat::OwnedModelSet owned_models;
	MissionDefinitions definitions;
	xl_assets::ObjectDependencies graph;
	bool has_hxm = false;
	bool installed = false;
	bool mission_definitions = false;
	int camera_robot = -1, camera_model = -1;
	HxmChanges changes;
	xl_compat::MissionLightingCompanions lighting;
	std::shared_ptr<const xl_compat::MessageResources> messages;
	std::shared_ptr<const xl_compat::AudioResources> audio;
	std::unique_ptr<xl_compat::PreparedAudioInstall> audio_install;
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

	void prepare_models(int ordinal, mission_preparation *target = nullptr)
	{
		owned_models = complete_mission ? prepare_owned_models(ordinal, target) : xl_prepare_owned_models(ordinal);
		const auto &sources = complete_mission ? definition_sources : mission_definition_sources;
		owned_models.sources.insert(sources.begin(), sources.end());
		for (const auto &entry : owned_models.models) {
			const int id = entry.first;
			if (changes.models.count(id)) continue;
			if (std::size_t(id) < definitions.models.size() && definitions.models[id].model_data && (complete_mission || !mission_generated_models.count(id))) continue;
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
			if (complete_mission) generated_models.insert(id);
			has_hxm = true;
		}
		bind_graph();
		prepare_native_models();
		if (mission_definitions) publication = { owned_models, definition_sources, generated_models };
	}

	explicit State(MissionDefinitions base) : definitions(std::move(base))
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
		definitions.bind_dependencies(graph);
		graph.resource_banks = banks.get();
		graph.require_owned_banks = mission_definitions;
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

XlPreparedResources::XlPreparedResources(XlPreparedHam incoming)
{
	state = std::make_unique<State>(MissionDefinitions(incoming.tables()));
	state->incoming_ham = std::make_unique<XlPreparedHam>(std::move(incoming));
	state->mission_definitions = true;
	state->bind_graph();
}

std::unique_ptr<XlPreparedResources> XlPreparedResources::prepare_mission(mission_preparation *mission)
{
	if (mission && (!mission_prepared_descriptor(mission) || mission_prepared_descriptor(mission)->descent_version != 2))
		throw std::runtime_error("Incoming resource preparation requires a D2 descriptor");
	const auto ham = owned_mod_file(".ham", {}, mission);
	XlPreparedHam incoming = [&] {
		if (ham.empty()) {
			if (startup_ham_hash.empty() || startup_origin.empty())
				throw std::runtime_error("Incoming mission has no trusted startup HAM origin");
			auto bytes = xl_retain_origin_resource(startup_origin, startup_ham_name);
			if (picosha2::hash256_hex_string(bytes) != startup_ham_hash)
				throw std::runtime_error("Captured startup HAM changed before incoming preparation");
			return XlPreparedHam(std::move(bytes));
		}
		auto file = incoming_owned_file(mission, ham);
		return XlPreparedHam(file.get());
	}();
	const auto hash = incoming.source_hash();
	auto result = std::make_unique<XlPreparedResources>(std::move(incoming));
	result->state->complete_mission = true;
	result->state->definition_sources[ham.empty() ? resource_key(startup_ham_name) : ham] = hash;
	const auto hxm = owned_mod_file(".hxm", {}, mission);
	if (!hxm.empty()) {
		auto replacement = incoming_owned_file(mission, hxm);
		result->state->definition_sources[hxm] = result->prepare_hxm(replacement.get());
	}
	const auto vham = owned_mod_file(".vham", {}, mission);
	if (!vham.empty()) {
		auto extension = incoming_owned_file(mission, vham);
		result->state->definition_sources[vham] = result->prepare_vham(extension.get());
	}
	result->state->prepare_models(0, mission);
	return result;
}

const std::map<std::string, std::string> &XlPreparedResources::sources() const
{
	return state->owned_models.sources;
}

std::unique_ptr<XlPreparedResources> XlPreparedResources::prepare_level(mission_preparation *mission, int ordinal)
{
	xl_compat::TargetSources target(mission, startup_origin);
	const char *filename = mission_prepared_level(mission, ordinal);
	if (!filename) throw std::runtime_error("Incoming resource level is absent from its descriptor");
	auto bytes = target.archive_bytes(filename, 64 * 1024 * 1024);
	if (!bytes) throw std::runtime_error("Incoming level is missing from its target HOG");
	auto inventory = std::make_unique<xl_compat::Inventory>(xl_compat::inspect_level(*bytes));
	auto result = prepare_mission(mission);
	result->prepare_level_resources(filename, inventory.get(), ordinal, mission, &target);
	xl_validate_incoming_requirements(*inventory, false, false, result->state->graph, mission, target);
	result->state->level_bytes = std::move(*bytes);
	result->state->level_inventory = std::move(inventory);
	return result;
}

const xl_compat::Inventory &XlPreparedResources::level_inventory() const
{
	if (!state || !state->level_inventory) throw std::runtime_error("Resource owner has no retained incoming level");
	return *state->level_inventory;
}

XlPreparedResources::XlPreparedResources(XlPreparedResources &&other) noexcept : state(std::move(other.state))
{
	if (installed_resources == &other) installed_resources = this;
}
XlPreparedResources &XlPreparedResources::operator=(XlPreparedResources &&other) noexcept
{
	if (this == &other) return *this;
	if (installed_resources == this) installed_resources = nullptr;
	state = std::move(other.state);
	if (installed_resources == &other) installed_resources = this;
	return *this;
}

void XlPreparedResources::prepare_banks(const std::string &palette)
{
	if (state->installed) throw std::runtime_error("Cannot prepare banks after resource publication");
	if (state->pog) throw std::runtime_error("Banks must be prepared before POG admission");
	const auto names = graphics_bank_names(palette);
	auto palette_bytes = retained_bank_source(names.first);
	std::size_t bank = GameArg.SndDigiSampleRate == 22050 ? 1 : 0;
	if (startup_sound_hashes[bank].empty()) bank = 1 - bank;
	const auto bitmap_source = startup_bank_sources.find(names.second);
	const auto sound_source = startup_bank_sources.find(sound_banks[bank]);
	if (bitmap_source == startup_bank_sources.end() || sound_source == startup_bank_sources.end())
		throw std::runtime_error("No qualified startup bitmap or sound bank origin");
	auto bitmap_bytes = xl_retain_origin_resource(bitmap_source->second.origin, bitmap_source->second.member);
	auto sound_bytes = xl_retain_origin_resource(sound_source->second.origin, sound_source->second.member);
	const int rate = bank ? 22050 : 11025;
	auto prepared = prepared_bank_cache;
	// Compare every reread byte against the previously hash-verified immutable owner
	if (!prepared || prepared->bitmap_hash() != bitmap_source->second.hash ||
	    prepared->sound_hash() != sound_source->second.hash || !prepared->matches(bitmap_bytes, sound_bytes, rate)) {
		if (picosha2::hash256_hex_string(bitmap_bytes) != bitmap_source->second.hash ||
		    picosha2::hash256_hex_string(sound_bytes) != sound_source->second.hash)
			throw std::runtime_error("Qualified startup bitmap or sound bank changed");
		prepared = std::make_shared<XlResourceBanks>(std::move(bitmap_bytes), std::move(sound_bytes), rate);
		prepared_bank_cache = prepared;
	}
	state->banks = std::move(prepared);
	state->palette_bytes = std::move(palette_bytes);
	state->bind_graph();
}

std::string XlPreparedResources::prepare_hxm(PHYSFS_file *file)
{
	if (state->installed) throw std::runtime_error("Cannot prepare HXM after resource publication");
	if (state->pog) throw std::runtime_error("HXM must be prepared before POG admission");
	const auto &graph = state->graph;
	auto retained = xl_retain_resource(file);
	auto staged = std::make_unique<State>(state->definitions);
	staged->changes = state->changes;
	xl_read_retained_resource(retained, [&](PHYSFS_file *input) {
		if (!bm_validate_hxm_for_tables(input, graph.robot_count, graph.joint_count, graph.model_count, 0))
			throw std::runtime_error("HXM definitions do not match the target tables");
		validate_hxm_dependencies(input, staged->definitions, staged->graph, staged->changes, false, true);
	});
	staged->prepare_native_models();
	auto hash = picosha2::hash256_hex_string(retained);
	state->definition_layers.push_back(std::move(retained));
	state->definitions = std::move(staged->definitions);
	state->changes = std::move(staged->changes);
	state->native_models = std::move(staged->native_models);
	state->bind_graph();
	state->has_hxm = true;
	return hash;
}

std::string XlPreparedResources::prepare_vham(PHYSFS_file *file)
{
	if (state->installed) throw std::runtime_error("Cannot prepare VHAM after resource publication");
	if (state->pog) throw std::runtime_error("VHAM must be prepared before POG admission");
	auto extra = std::make_unique<XlPreparedHam>(XlPreparedHam::extra(file));
	const auto &target = extra->tables();
	auto staged = std::make_unique<State>(state->definitions);
	staged->changes = state->changes;
	auto &definitions = staged->definitions;
	auto &changes = staged->changes;
	// Native VHAM replaces suffixes at fixed stock ordinals, after mission HXM
	auto replace = [](auto &values, const auto *source, int first, int total, auto &changed) {
		if (int(values.size()) < first) throw std::runtime_error("Incoming HAM lacks the native VHAM base tables");
		values.resize(total);
		changed.erase(changed.lower_bound(total), changed.end());
		for (int id = first; id < total; ++id) {
			values[id] = source[id];
			changed.insert(id);
		}
	};
	replace(definitions.weapons, target.Weapon_info, N_D2_WEAPON_TYPES, *target.N_weapon_types, changes.weapons);
	replace(definitions.robots, target.Robot_info, N_D2_ROBOT_TYPES, *target.N_robot_types, changes.robots);
	replace(definitions.joints, target.Robot_joints, N_D2_ROBOT_JOINTS, *target.N_robot_joints, changes.joints);
	replace(definitions.models, target.Polygon_models, N_D2_POLYGON_MODELS, *target.N_polygon_models, changes.models);
	definitions.payloads.resize(definitions.models.size());
	definitions.dying.resize(definitions.models.size());
	definitions.dead.resize(definitions.models.size());
	definitions.base_radii.resize(definitions.models.size());
	definitions.custom_radii.resize(definitions.models.size());
	for (int id = N_D2_POLYGON_MODELS; id < *target.N_polygon_models; ++id) {
		auto &model = definitions.models[id];
		auto &payload = definitions.payloads[id];
		payload.assign(model.model_data, model.model_data + model.model_data_size);
		model.model_data = payload.data();
		definitions.dying[id] = target.Dying_modelnums[id];
		definitions.dead[id] = target.Dead_modelnums[id];
		definitions.base_radii[id] = model.rad;
		definitions.custom_radii[id] = 0;
	}
	for (int id = N_D2_OBJBITMAPS; id < N_D2_OBJBITMAPS + extra->extra_bitmap_count(); ++id) {
		definitions.bitmaps[id] = target.ObjBitmaps[id];
		changes.bitmaps.insert(id);
	}
	for (int id = N_D2_OBJBITMAPPTRS; id < N_D2_OBJBITMAPPTRS + extra->extra_pointer_count(); ++id) {
		definitions.pointers[id] = target.ObjBitmapPtrs[id];
		changes.pointers.insert(id);
	}
	staged->bind_graph();
	staged->prepare_native_models();
	auto hash = extra->source_hash();
	state->definitions = std::move(staged->definitions);
	state->changes = std::move(staged->changes);
	state->native_models = std::move(staged->native_models);
	state->incoming_extra = std::move(extra);
	state->bind_graph();
	state->has_hxm = true;
	return hash;
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
	prepare_level_resources(level_filename, level, ordinal, nullptr, nullptr);
}

void XlPreparedResources::prepare_level_resources(const char *level_filename, const xl_compat::Inventory *level, int ordinal, mission_preparation *mission, xl_compat::TargetSources *target)
{
	if (target) ordinal = target->ordinal(level_filename, ordinal);
	if (level) prepare_banks(level->palette);
	if (!ordinal) {
		for (int candidate = Last_secret_level; candidate <= Last_level; ++candidate) {
			if (!candidate) continue;
			const char *name = candidate < 0 ? Secret_level_names[-candidate - 1] : Level_names[candidate - 1];
			if (d_stricmp(level_filename, name)) continue;
			if (ordinal) throw std::runtime_error("XL resources require an explicit incoming level number for a repeated level filename");
			ordinal = candidate;
		}
	}
	if (level) state->messages = xl_compat::prepare_messages(level_filename, *level, target);
	if (level) state->audio = xl_compat::prepare_named_audio(level_filename, *level, ordinal, state->banks.get(), target);
	if (level) {
		state->particle_image = xl_compat::prepare_particle_image(*level, 3, target);
		state->smoke_image = xl_compat::prepare_particle_image(*level, 0, target);
		state->bubble_image = xl_compat::prepare_particle_image(*level, 2, target);
		state->rain_image = xl_compat::prepare_particle_image(*level, 5, target);
		state->snow_image = xl_compat::prepare_particle_image(*level, 6, target);
		state->lightning_image = xl_compat::prepare_lightning_image(*level, target);
		state->powerup_glare_image = xl_compat::prepare_powerup_glare_image(target);
		if (xl_runtime::time_equipment_resources_required(*level))
			for (unsigned i = 0; i < 2; ++i) state->time_equipment_images[i] = xl_compat::prepare_time_equipment_image(i, target);
	}
	std::string base = level_filename;
	const auto dot = base.find_last_of('.');
	if (dot != std::string::npos) base.resize(dot);
	if (target || PHYSFSX_exists((base + ".lgt").c_str(), 1) || PHYSFSX_exists((base + ".clr").c_str(), 1)) {
		if (target ? target->archive().empty() : !*mission_resource_archive())
			throw std::runtime_error("D2 XL lighting companions require an owned mission HOG: " + base);
		state->lighting = xl_compat::load_mission_lighting_companions(level_filename, target);
		// The pinned modern loader reads CLR first, then replaces every texture color
		// from the embedded table. Ownership and decoding still apply to the companion
		if (state->lighting.colors && !(level && xl_compat::modern_runtime_layout(*level) && level->texture_colors.size() == xl_compat::companion_texture_count))
			throw std::runtime_error("D2 XL CLR companion overrides are not supported: " + std::string(level_filename));
	}
	const auto hxm_name = base + ".HXM";
	const auto prepare_level_hxm = [&](PHYSFS_file *file) {
		if (!bm_validate_hxm_for_tables(file, state->graph.robot_count, state->graph.joint_count, state->graph.model_count, 1))
			throw std::runtime_error("HXM definitions are malformed or require unsupported normalization: " + hxm_name);
		try {
			validate_hxm_dependencies(file, state->definitions, state->graph, state->changes, false, false, true);
			state->has_hxm = true;
			state->prepare_native_models();
		} catch (const std::exception &error) {
			throw std::runtime_error("HXM dependencies: " + hxm_name + ": " + error.what());
		}
	};
	if (target) {
		if (const auto bytes = target->archive_bytes(hxm_name, 64 * 1024 * 1024)) xl_read_retained_resource(*bytes, prepare_level_hxm);
	} else if (auto file = companion(hxm_name)) prepare_level_hxm(file.get());
	state->prepare_models(ordinal, mission);
	if (level && std::any_of(level->objects.begin(), level->objects.end(), [](const xl_compat::Object &object) { return object.type == 16; })) {
		if (!target) prepare_camera_model();
		else {
			auto camera = target->archive_bytes("cambot.hxm", 64 * 1024 * 1024);
			if (!camera) camera = target->directory_bytes("cambot.hxm", 64 * 1024 * 1024);
			if (!camera) camera = target->directory_bytes("cambot.hxm", 64 * 1024 * 1024, true);
			if (!camera) {
				for (const char *path : { "data/d2x-xl.hog", "d2x-xl.hog" }) {
					auto archive = target->directory_bytes(path, 64 * 1024 * 1024);
					if (!archive) archive = target->directory_bytes(path, 64 * 1024 * 1024, true);
					if (!archive) continue;
					camera = xl_find_archive_resource(*archive, ".hog", "cambot.hxm", 64 * 1024 * 1024);
					if (!camera) throw std::runtime_error("Incoming camera addon lacks cambot.hxm");
					break;
				}
			}
			if (!camera) throw std::runtime_error("Missing incoming target or startup camera addon");
			xl_read_retained_resource(*camera, [&](PHYSFS_file *file) { prepare_camera_model(file); });
		}
	}
	const auto pog_name = base + ".POG";
	const auto prepare_level_pog = [&](PHYSFS_file *file) {
		try {
			prepare_pog(file);
		} catch (const std::exception &error) {
			throw std::runtime_error(pog_name + ": " + error.what());
		}
	};
	if (target) {
		if (const auto bytes = target->archive_bytes(pog_name, 64 * 1024 * 1024)) xl_read_retained_resource(*bytes, prepare_level_pog);
	} else if (auto file = companion(pog_name)) prepare_level_pog(file.get());
	state->audio_install = std::make_unique<xl_compat::PreparedAudioInstall>(state->audio);
}

void XlPreparedResources::prepare_pog(PHYSFS_file *file)
{
	if (state->installed) throw std::runtime_error("Cannot prepare POG after resource publication");
	if (state->mission_definitions && !state->banks) throw std::runtime_error("Incoming POG requires prepared target banks");
	char error[256]{};
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> prepared(xl_pog_prepare(file, state->banks ? state->banks->bitmap_count() : Num_bitmap_files, error, sizeof(error)), xl_pog_free);
	if (!prepared) throw std::runtime_error(std::string("POG: ") + error);
	for (int i = 0; i < xl_pog_count(prepared.get()); ++i) {
		const auto *entry = xl_pog_entry(prepared.get(), i);
		if (!entry->rgba) continue;
		if (entry->frames > 1) {
			const int clock = xl_pog_target_strip_clock(entry->index, state->graph);
			if (clock < 0) throw std::runtime_error(std::string("POG: native bitmap animation tables exceed supported bounds"));
			if (clock) throw std::runtime_error(std::string("POG: RGBA bitmap ") + std::to_string(entry->index) +
				                                " has clocked animation frames; playback is not implemented");
		}
		bool cockpit = false;
		for (const auto &bitmap : state->definitions.cockpits) cockpit = cockpit || bitmap.index == entry->index;
		for (int gauge = 0; gauge < MAX_GAUGE_BMS; ++gauge)
			cockpit = cockpit || state->definitions.gauges[gauge].index == entry->index || state->definitions.gauges_hires[gauge].index == entry->index;
		if (cockpit) throw std::runtime_error(std::string("POG: RGBA bitmap ") + std::to_string(entry->index) +
			                                  " replaces a cockpit/HUD image; true-color cockpit layout is not implemented");
		if (const char *reason = ogl_xl_rgba_prepare(int(entry->width), int(entry->frame_height)))
			throw std::runtime_error(std::string("POG: ") + reason);
	}
	if (!xl_pog_prepare_storage(prepared.get())) throw std::runtime_error("Cannot prepare native POG pixel storage");
	state->pog = std::move(prepared);
}

void XlPreparedResources::prepare_camera_model(PHYSFS_file *file)
{
	xl_camera_validate_weapon_definitions(state->graph.weapon_data, state->graph.weapon_count);
	if (state->camera_model >= 0) return;
	if (state->pog) throw std::runtime_error("Camera definitions must be prepared before POG admission");
	if (state->installed) throw std::runtime_error("Camera resources must be prepared before installation");
	std::unique_ptr<CameraAddonFile> addon;
	if (!file) {
		addon = std::make_unique<CameraAddonFile>();
		file = addon->file.get();
	}
	if (!bm_validate_hxm_for_tables(file, state->graph.robot_count, state->graph.joint_count, state->graph.model_count, 0))
		throw std::runtime_error("Camera addon HXM is malformed or uses unqualified serialized indices");
	// Prepare transactionally; a rejected addon leaves this owner's prior definitions intact
	auto staged = std::make_unique<State>(state->definitions);
	staged->graph = state->graph;
	const auto ids = validate_hxm_dependencies(file, staged->definitions, staged->graph, staged->changes, true);
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
	// Inline definition arrays move by value; bind views to the surviving owner
	state->definitions.bind_dependencies(state->graph);
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
	if (!xl_pog_install_prepared(state->pog.get())) throw std::runtime_error("Prepared POG storage or animation tables are unavailable");
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
	if (!state->audio_install) throw std::runtime_error("Level audio publication has not been prepared");
	state->audio_install->validate();
	xl_compat::install_messages(state->messages);
	state->audio_install->install();
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
	if (state->mission_definitions && state->installed)
		throw std::runtime_error("Incoming definitions have already been published");
	std::unique_ptr<State::Publication> selected_publication;
	auto *publication = &state->publication;
	if (state->mission_definitions) {
		// All incoming model buffers and registry nodes were allocated during preparation
		for (const int id : state->changes.models)
			if (!state->native_models[id] || !state->native_models[id]->model.model_data)
				throw std::runtime_error("Incoming model publication storage is unavailable");
	} else {
		selected_publication = std::make_unique<State::Publication>(State::Publication{ state->owned_models, state->definition_sources, state->generated_models });
		publication = selected_publication.get();
		// Selected-mission reloads may have freed the previously transferred buffers
		for (const int id : state->changes.models)
			if (!state->native_models[id]->model.model_data) {
				state->prepare_native_models();
				break;
			}
	}
	if (state->incoming_ham) {
		state->incoming_ham->install();
		std::memcpy(Pof_names, state->definitions.model_names, sizeof(Pof_names));
		state->incoming_ham.reset();
	}
	if (state->complete_mission) {
		mission_definition_sources.swap(publication->definition_sources);
		mission_generated_models.swap(publication->generated_models);
	}
	if (!state->has_hxm) {
		xl_oof_renderer_clear();
		mission_oof_models.swap(publication->owned_models.models);
		mission_mod_sources.swap(publication->owned_models.sources);
		state->installed = true;
		installed_resources = this;
		return;
	}
	state->hitboxes.clear();
	auto &definitions = state->definitions;
	if (state->incoming_extra) {
		bm_free_extra_objbitmaps();
		for (int id = int(definitions.models.size()); id < N_polygon_models; ++id) {
			free_model(&Polygon_models[id]);
			Polygon_models[id].model_data = nullptr;
		}
	}
	for (const int id : state->changes.weapons) Weapon_info[id] = definitions.weapons[id];
	if (state->incoming_extra) N_weapon_types = int(definitions.weapons.size());
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
	if (!state->mission_definitions) Robot_replacements_loaded = 1;
	xl_oof_renderer_clear();
	mission_oof_models.swap(publication->owned_models.models);
	mission_mod_sources.swap(publication->owned_models.sources);
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
	// Register after the native memory tracker, so retained native allocations
	// retire before its exit sweep and before static C++ owner destruction
	static const bool cleanup_registered = [] {
		if (std::atexit(xl_discard_prepared_resources) != 0)
			throw std::runtime_error("Cannot register prepared resource shutdown");
		return true;
	}();
	(void) cleanup_registered;
	if (!resources || !level_filename) throw std::runtime_error("Prepared level publication requires an owner and name");
	std::string level(level_filename);
	resources->install_messages();
	prepared_level.swap(level);
	prepared_resources = std::move(resources);
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
