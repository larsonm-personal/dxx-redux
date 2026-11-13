#include <array>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "prepared_ham.h"
extern "C" {
#include "gauges.h"
#include "sounds.h"
#include "u_mem.h"
}

struct XlPreparedHam::State {
	std::vector<ubyte> source_bytes;
	int NumTextures{};
	std::array<bitmap_index, MAX_TEXTURES> Textures{};
	std::array<tmap_info, MAX_TEXTURES> TmapInfo{};
	std::array<ubyte, MAX_SOUNDS> Sounds{};
	std::array<ubyte, MAX_SOUNDS> AltSounds{};
	int Num_vclips{};
	std::array<vclip, VCLIP_MAXNUM> Vclip{};
	int Num_effects{};
	std::array<eclip, MAX_EFFECTS> Effects{};
	int Num_wall_anims{};
	std::array<wclip, MAX_WALL_ANIMS> WallAnims{};
	int N_robot_types{};
	std::array<robot_info, MAX_ROBOT_TYPES> Robot_info{};
	int N_robot_joints{};
	std::array<jointpos, MAX_ROBOT_JOINTS> Robot_joints{};
	int N_weapon_types{};
	std::array<weapon_info, MAX_WEAPON_TYPES> Weapon_info{};
	int N_powerup_types{};
	std::array<powerup_type_info, MAX_POWERUP_TYPES> Powerup_info{};
	int N_polygon_models{};
	std::array<polymodel, MAX_POLYGON_MODELS> Polygon_models{};
	std::array<int, MAX_POLYGON_MODELS> Dying_modelnums{};
	std::array<int, MAX_POLYGON_MODELS> Dead_modelnums{};
	std::array<bitmap_index, MAX_GAUGE_BMS> Gauges{};
	std::array<bitmap_index, MAX_GAUGE_BMS> Gauges_hires{};
	int N_ObjBitmaps{};
	std::array<bitmap_index, MAX_OBJ_BITMAPS> ObjBitmaps{};
	std::array<ushort, MAX_OBJ_BITMAPS> ObjBitmapPtrs{};
	player_ship only_player_ship{};
	int Num_cockpits{};
	std::array<bitmap_index, N_COCKPIT_BITMAPS> cockpit_bitmap{};
	int First_multi_bitmap_num{};
	int Num_reactors{};
	std::array<reactor, MAX_REACTORS> Reactors{};
	int Marker_model_num{};
	int exit_modelnum{};
	int destroyed_exit_modelnum{};
	bm_ham_destination destination{
		&NumTextures,
		Textures.data(),
		TmapInfo.data(),
		Sounds.data(),
		AltSounds.data(),
		&Num_vclips,
		Vclip.data(),
		&Num_effects,
		Effects.data(),
		&Num_wall_anims,
		WallAnims.data(),
		&N_robot_types,
		Robot_info.data(),
		&N_robot_joints,
		Robot_joints.data(),
		&N_weapon_types,
		Weapon_info.data(),
		&N_powerup_types,
		Powerup_info.data(),
		&N_polygon_models,
		Polygon_models.data(),
		Dying_modelnums.data(),
		Dead_modelnums.data(),
		Gauges.data(),
		Gauges_hires.data(),
		&N_ObjBitmaps,
		ObjBitmaps.data(),
		ObjBitmapPtrs.data(),
		&only_player_ship,
		&Num_cockpits,
		cockpit_bitmap.data(),
		&First_multi_bitmap_num,
		&Num_reactors,
		Reactors.data(),
		&Marker_model_num,
		&exit_modelnum,
		&destroyed_exit_modelnum,
	};
	~State()
	{
		for (auto &model : Polygon_models)
			if (model.model_data) free_model(&model);
	}
};

namespace
{
// Adapt retained bytes to the existing PHYSFS-based native codec without a
// second HAM parser or a temporary on-disk file. This is not an archive format:
// only the exact in-progress private mount name is recognized, and registration
// lasts for the synchronous decode. Nothing is published at a resource basename
std::mutex snapshot_mutex;
thread_local const char *snapshot_name;

void *snapshot_open(PHYSFS_Io *io, const char *name, int writing, int *claimed)
{
	if (writing || !snapshot_name || std::strcmp(name, snapshot_name)) return nullptr;
	*claimed = 1;
	return io->seek(io, 0) ? io : nullptr;
}

PHYSFS_EnumerateCallbackResult snapshot_enumerate(void *, const char *directory, PHYSFS_EnumerateCallback callback, const char *original, void *context)
{
	if (*directory) return PHYSFS_ENUM_OK;
	const auto result = callback(context, original, "data");
	if (result == PHYSFS_ENUM_ERROR) PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
	return result;
}

PHYSFS_Io *snapshot_read(void *opaque, const char *name)
{
	if (std::strcmp(name, "data")) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return nullptr;
	}
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	auto *copy = source->duplicate(source);
	if (copy && !copy->seek(copy, 0)) {
		copy->destroy(copy);
		return nullptr;
	}
	return copy;
}

PHYSFS_Io *snapshot_write(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return nullptr;
}
int snapshot_modify(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return 0;
}
int snapshot_stat(void *opaque, const char *name, PHYSFS_Stat *stat)
{
	if (*name && std::strcmp(name, "data")) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return 0;
	}
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	*stat = { *name ? source->length(source) : -1, -1, -1, -1,
		      *name ? PHYSFS_FILETYPE_REGULAR : PHYSFS_FILETYPE_DIRECTORY, 1 };
	return 1;
}
void snapshot_close(void *opaque)
{
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	source->destroy(source);
}

const PHYSFS_Archiver snapshot_archiver = {
	0, { "XLHAMSNAP", "Private retained HAM bytes", "DXX Redux", "", 0 }, snapshot_open, snapshot_enumerate, snapshot_read, snapshot_write, snapshot_write, snapshot_modify, snapshot_modify, snapshot_stat, snapshot_close
};

bool decode_snapshot(const std::vector<ubyte> &bytes, const bm_ham_destination &destination,
                     int (*read_model)(polymodel *, PHYSFS_file *))
{
	std::lock_guard<std::mutex> lock(snapshot_mutex);
	static unsigned long serial;
	std::string name;
	do {
		name = "__xl_ham_snapshot_" + std::to_string(++serial) + ".XLHAMSNAP";
	} while (PHYSFS_exists(name.c_str()) || PHYSFS_getMountPoint(name.c_str()));
	if (!PHYSFS_registerArchiver(&snapshot_archiver)) throw std::runtime_error("Cannot register retained HAM reader");
	struct Registration {
		bool active = true;
		~Registration()
		{
			snapshot_name = nullptr;
			if (active) PHYSFS_deregisterArchiver("XLHAMSNAP");
		}
	} registration;
	void *memory = std::malloc(bytes.size());
	if (!memory) throw std::bad_alloc();
	std::memcpy(memory, bytes.data(), bytes.size());
	snapshot_name = name.c_str();
	const bool mounted = PHYSFS_mountMemory(memory, bytes.size(), std::free, name.c_str(), name.c_str(), 1) != 0;
	snapshot_name = nullptr;
	if (!mounted) {
		std::free(memory);
		throw std::runtime_error("Cannot mount retained HAM bytes");
	}
	struct Mount {
		const std::string &name;
		bool active = true;
		~Mount()
		{
			if (active) PHYSFS_unmount(name.c_str());
		}
	} mount{ name };
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead((name + "/data").c_str()), PHYSFS_close);
	if (!file) throw std::runtime_error("Cannot open retained HAM bytes");
	const bool decoded = bm_read_xl_ham_into(file.get(), &destination, read_model) != 0;
	if (!PHYSFS_close(file.get())) throw std::runtime_error("Cannot close retained HAM reader");
	file.release();
	if (!PHYSFS_unmount(name.c_str())) throw std::runtime_error("Cannot release retained HAM mount");
	mount.active = false;
	if (!PHYSFS_deregisterArchiver("XLHAMSNAP")) throw std::runtime_error("Cannot release retained HAM reader registration");
	registration.active = false;
	return decoded;
}

int read_owned_model(polymodel *model, PHYSFS_file *file)
{
	if (model->model_data_size <= 0) return 0;
	model->model_data = static_cast<ubyte *>(d_malloc(model->model_data_size));
	if (!model->model_data) return 0;
	if (PHYSFS_readBytes(file, model->model_data, model->model_data_size) != model->model_data_size) return 0;
	polygon_model_data_init_scoped(model);
	return 1;
}
} // namespace

XlPreparedHam::XlPreparedHam(mission_preparation *mission, const char *member) : state(std::make_unique<State>())
{
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(mission_prepared_open(mission, member), PHYSFS_close);
	if (!file) throw std::runtime_error(std::string("Cannot open target mission HAM: ") + member);
	const auto length = PHYSFS_fileLength(file.get());
	if (length <= 0 || length > 64 * 1024 * 1024) throw std::runtime_error("Target mission HAM exceeds retained source bounds");
	state->source_bytes.resize(static_cast<std::size_t>(length));
	if (PHYSFS_readBytes(file.get(), state->source_bytes.data(), length) != length)
		throw std::runtime_error("Cannot retain target mission HAM bytes");
	file.reset();
	if (!decode_snapshot(state->source_bytes, state->destination, read_owned_model))
		throw std::runtime_error(std::string("Cannot decode target mission HAM: ") + member);
}

XlPreparedHam::~XlPreparedHam() = default;
XlPreparedHam::XlPreparedHam(XlPreparedHam &&) noexcept = default;
XlPreparedHam &XlPreparedHam::operator=(XlPreparedHam &&) noexcept = default;
const bm_ham_destination &XlPreparedHam::tables() const
{
	return state->destination;
}
