#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "picosha2.h"
#include "retained_resource.h"
#include "prepared_ham.h"
extern "C" {
#include "gauges.h"
#include "sounds.h"
#include "u_mem.h"
}

struct XlPreparedHam::State {
	bool installed = false, extra = false;
	int extra_bitmaps = 0, extra_pointers = 0;
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

XlPreparedHam::XlPreparedHam(mission_preparation *mission, const char *member)
{
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(mission_prepared_open(mission, member), PHYSFS_close);
	if (!file) throw std::runtime_error(std::string("Cannot open target mission HAM: ") + member);
	*this = XlPreparedHam(file.get());
}

XlPreparedHam::XlPreparedHam(PHYSFS_file *file) : XlPreparedHam(xl_retain_resource(file)) {}

XlPreparedHam::XlPreparedHam(std::vector<unsigned char> bytes) : state(std::make_unique<State>())
{
	state->source_bytes = std::move(bytes);
	xl_read_retained_resource(state->source_bytes, [&](PHYSFS_file *retained) {
		if (!bm_read_xl_ham_into(retained, &state->destination, read_owned_model))
			throw std::runtime_error("Cannot decode target mission HAM");
	});
}

XlPreparedHam::XlPreparedHam() : state(std::make_unique<State>()) {}

XlPreparedHam XlPreparedHam::extra(PHYSFS_file *file)
{
	XlPreparedHam result;
	result.state->extra = true;
	result.state->source_bytes = xl_retain_resource(file);
	xl_read_retained_resource(result.state->source_bytes, [&](PHYSFS_file *retained) {
		if (!bm_read_xl_extra_ham_into(retained, &result.state->destination, read_owned_model,
		                               &result.state->extra_bitmaps, &result.state->extra_pointers))
			throw std::runtime_error("Cannot decode target mission VHAM");
	});
	return result;
}

int XlPreparedHam::extra_bitmap_count() const
{
	if (!state || !state->extra) throw std::runtime_error("Not an extra HAM owner");
	return state->extra_bitmaps;
}
int XlPreparedHam::extra_pointer_count() const
{
	if (!state || !state->extra) throw std::runtime_error("Not an extra HAM owner");
	return state->extra_pointers;
}

XlPreparedHam::~XlPreparedHam() = default;
XlPreparedHam::XlPreparedHam(XlPreparedHam &&) noexcept = default;
XlPreparedHam &XlPreparedHam::operator=(XlPreparedHam &&) noexcept = default;
const bm_ham_destination &XlPreparedHam::tables() const
{
	if (!state || state->installed) throw std::runtime_error("Target mission HAM no longer owns definitions");
	return state->destination;
}

void XlPreparedHam::install()
{
	if (!state || state->installed || state->extra) throw std::runtime_error("Target mission HAM cannot be published as a complete base");
	bm_install_xl_ham(&state->destination);
	state->installed = true;
}

std::string XlPreparedHam::source_hash() const
{
	if (!state) throw std::runtime_error("Target mission HAM was moved");
	return picosha2::hash256_hex_string(state->source_bytes);
}
