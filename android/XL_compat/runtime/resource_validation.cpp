#include "resource_validation.h"
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
#include "companion_resources.h"
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
extern int Robot_replacements_loaded;
}

namespace
{
using File = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
std::string startup_ham_name, startup_ham_hash, base_rejection;
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

	MissionDefinitions() : payloads(models.size())
	{
		for (std::size_t id = 0; id < models.size(); ++id) {
			auto &model = models[id];
			if (model.model_data && model.model_data_size > 0) {
				payloads[id].assign(model.model_data, model.model_data + model.model_data_size);
				model.model_data = payloads[id].data();
			}
		}
	}
};
std::unique_ptr<MissionDefinitions> mission_base;
std::unique_ptr<XlPreparedResources> prepared_resources;
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

void validate_hxm_dependencies(PHYSFS_file *file, MissionDefinitions &definitions, xl_assets::ObjectDependencies &graph, HxmChanges &changes)
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
	PHYSFS_seek(file, 8);
	for (int count = PHYSFSX_readInt(file); count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		robot_info_read_n(&robots.at(id), 1, file);
		changed_robots.insert(id);
	}
	for (int count = PHYSFSX_readInt(file); count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		jointpos_read_n(&joints.at(id), 1, file);
		changed_joints.insert(id);
	}
	for (int count = PHYSFSX_readInt(file); count > 0; --count) {
		const int id = PHYSFSX_readInt(file);
		auto &model = models.at(id);
		polymodel_read(&model, file);
		auto &payload = payloads.at(id);
		payload.resize(model.model_data_size);
		if (PHYSFS_read(file, payload.data(), 1, static_cast<PHYSFS_uint32>(payload.size())) != static_cast<PHYSFS_sint64>(payload.size()))
			throw std::runtime_error("HXM model payload read failed");
		model.model_data = payload.data();
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
	for (const int id : changed_models) graph.model(id);
	for (const int id : changed_bitmaps) graph.bitmap(bitmaps[id].index);
	for (const int id : changed_pointers) {
		if (pointers[id] >= N_ObjBitmaps) throw std::runtime_error("HXM model texture references missing object bitmap");
		graph.bitmap(bitmaps[pointers[id]].index);
	}
	// A changed joint/model can invalidate an otherwise unchanged robot
	for (int id = 0; id < N_robot_types; ++id) {
		bool affected = changed_robots.count(id) || changed_models.count(robots[id].model_num);
		for (int gun = 0; gun <= MAX_GUNS; ++gun)
			for (int animation = 0; animation < N_ANIM_STATES; ++animation) {
				const auto state = robots[id].anim_states[gun][animation];
				for (const int joint : changed_joints)
					if (joint >= state.offset && joint - state.offset < state.n_joints) affected = true;
			}
		if (affected) graph.robot(id);
	}
}

void validate_pog(PHYSFS_file *file, const std::string &name)
{
	const auto length = PHYSFS_fileLength(file);
	if (length < 12 || length > 64 * 1024 * 1024)
		throw std::runtime_error("POG size is invalid: " + name);
	std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
	if (PHYSFS_read(file, bytes.data(), 1, static_cast<PHYSFS_uint32>(bytes.size())) != length)
		throw std::runtime_error("POG read failed: " + name);
	// The shared bounded reader also understands D1 formats; D2 only installs DPOG v1
	const unsigned char header[] = { 'D', 'P', 'O', 'G', 1, 0, 0, 0 };
	d1_custom_validation_limits limits{};
	limits.max_bitmaps = Num_bitmap_files;
	limits.max_sounds = MAX_SOUND_FILES;
	limits.rle_flag = BM_FLAG_RLE;
	limits.rle_big_flag = BM_FLAG_RLE_BIG;
	if (std::memcmp(bytes.data(), header, sizeof(header)) ||
	    !d1_custom_validate_pigpog(bytes.data(), bytes.size(), &limits, nullptr, nullptr, nullptr))
		throw std::runtime_error("POG bitmap indices, dimensions or encoded pixels are invalid: " + name);
}
} // namespace

extern "C" void xl_resource_note_startup_ham(const char *name, PHYSFS_file *file)
{
	try {
		startup_ham_hash = resource_hash(file, name);
		startup_ham_name = name;
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
	mission_base.reset();
}

extern "C" void xl_resource_baseline_capture(void)
{
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
	HxmChanges changes;
	xl_compat::MissionLightingCompanions lighting;
	std::vector<std::unique_ptr<NativeModel>> native_models;
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
	}
};

XlPreparedResources::~XlPreparedResources() = default;

xl_assets::ObjectDependencies &XlPreparedResources::dependencies()
{
	return state->graph;
}

XlPreparedResources::XlPreparedResources(const char *level_filename)
{
	if (!mission_base) throw std::runtime_error("clean mission definitions are unavailable");
	if (mission_base->robots.size() != std::size_t(N_robot_types))
		throw std::runtime_error("robot definition registry changed since mission initialization");
	if (mission_base->joints.size() != std::size_t(N_robot_joints))
		throw std::runtime_error("robot joint registry changed since mission initialization");
	if (mission_base->models.size() != std::size_t(N_polygon_models))
		throw std::runtime_error("model definition registry changed since mission initialization");
	state = std::make_unique<State>(*mission_base);
	std::string base = level_filename;
	const auto dot = base.find_last_of('.');
	if (dot != std::string::npos) base.resize(dot);
	if (PHYSFSX_exists((base + ".lgt").c_str(), 1) || PHYSFSX_exists((base + ".clr").c_str(), 1)) {
		if (!*mission_resource_archive())
			throw std::runtime_error("D2 XL lighting companions require an owned mission HOG: " + base);
		state->lighting = xl_compat::load_mission_lighting_companions(level_filename);
		if (state->lighting.colors)
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
		validate_pog(file.get(), pog_name);
		state->pog.reset(xl_pog_prepare(file.get()));
		if (!state->pog) throw std::runtime_error("cannot prepare POG bitmap storage: " + pog_name);
	}
}

void XlPreparedResources::install_pog()
{
	if (!xl_pog_install(state->pog.get())) throw std::runtime_error("cannot allocate prepared POG bitmap storage");
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

void XlPreparedResources::install_hxm()
{
	if (!state->has_hxm) return;
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
	Robot_replacements_loaded = 1;
}

void xl_set_prepared_resources(const char *level_filename, std::unique_ptr<XlPreparedResources> resources)
{
	prepared_level = level_filename;
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
