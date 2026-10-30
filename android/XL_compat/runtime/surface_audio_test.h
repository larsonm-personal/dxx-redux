#ifndef DXX_XL_SURFACE_AUDIO_TEST_H
#define DXX_XL_SURFACE_AUDIO_TEST_H
#include "gameplay_audio.h"
#include "linked_audio.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "shared_audio_internal.h"
#include "../tests/surface_audio_receipt.h"
#include <SDL_mixer.h>
#include <algorithm>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "bm.h"
#include "textures.h"
#include "effects.h"
#include "sounds.h"
#include "config.h"
#include "args.h"
#include "digi.h"
void set_sound_sources(void);
}
namespace xl_surface_audio_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
inline xl_surface_audio_receipt::Case receipt()
{
	using namespace xl_compat::audio_runtime;
	xl_surface_audio_receipt::Case value;
	value.custom_volume = int(GameCfg.DigiVolume) * 8192;
	value.sounds.assign(Sounds, Sounds + MAX_SOUNDS);
	for (int i = 0; i < NumTextures; ++i) value.textures.push_back(TmapInfo[i].eclip_num);
	for (int i = 0; i < Num_effects; ++i) value.effects.push_back(Effects[i].sound_num);
	value.segments.resize(unsigned(Num_segments));
	for (int segment = 0; segment < Num_segments; ++segment)
		for (int side = 0; side < 6; ++side) {
			const auto &face = Segments[segment].sides[side];
			vms_vector center;
			compute_center_point_on_side(&center, &Segments[segment], side);
			value.segments[unsigned(segment)][unsigned(side)] = { WALL_IS_DOORWAY(&Segments[segment], side), face.tmap_num, face.tmap_num2 & 0x3fff, Segments[segment].children[side], center.x, center.y, center.z };
		}
	AudioLock lock;
	for (unsigned i = 0; i < owners.count; ++i) {
		const auto &owner = owners.entries[i];
		check((owner.flags & xl_compat::sound_link_position) && owner.object == -1, "Surface setup created a non-surface owner");
		value.owners.push_back({ owner.sound, owner.segment, owner.side, owner.max_volume, owner.max_distance, int(owner.flags), owner.sound_class,
		                         owner.decay, owner.loop_start, owner.loop_end, owner.position[0], owner.position[1], owner.position[2] });
	}
	return value;
}
struct Guard {
	object *viewer = Viewer;
	int low_memory = GameArg.SysLowMem, volume = xl_compat::audio_runtime::priority_volume;
	decltype(GameCfg.DigiVolume) slider = GameCfg.DigiVolume;
	std::vector<segment> segments{ Segments, Segments + Num_segments };
	std::vector<tmap_info> textures{ TmapInfo, TmapInfo + NumTextures };
	std::vector<eclip> effects{ Effects, Effects + Num_effects };
	std::vector<wall> walls{ Walls, Walls + Num_walls };
	std::array<unsigned char, MAX_SOUNDS> sounds{};
	Guard()
	{
		std::copy_n(Sounds, MAX_SOUNDS, sounds.begin());
	}
	void restore()
	{
		std::copy(segments.begin(), segments.end(), Segments);
		std::copy(textures.begin(), textures.end(), TmapInfo);
		std::copy(effects.begin(), effects.end(), Effects);
		std::copy(walls.begin(), walls.end(), Walls);
		std::copy(sounds.begin(), sounds.end(), Sounds);
		Viewer = viewer;
		GameArg.SysLowMem = low_memory;
		GameCfg.DigiVolume = slider;
		xl_compat::audio_runtime::clear_linked_scene();
		digi_set_digi_volume(volume);
	}
	~Guard()
	{
		restore();
	}
};
template <class Output, class Record>
void run(Output output, Record record)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	check(Num_walls > 0 && NumTextures > 4 && Num_effects >= 3, "Surface controls need loaded textures, effects and walls");
	SDL_PauseAudio(1);
	const auto initial = named_audio_snapshot();
	object listener = *ConsoleObject;
	Guard guard;
	Viewer = &listener;
	GameArg.SysLowMem = 0;
	GameCfg.DigiVolume = 8;
	digi_set_digi_volume(32768);
	set_sound_sources();
	record(receipt());
	const int room = listener.segnum;
	int portal_side = -1, neighbor = -1, neighbor_side = -1;
	for (int side = 0; side < 6; ++side)
		if (Segments[room].children[side] >= 0) {
			portal_side = side;
			neighbor = Segments[room].children[side];
			neighbor_side = find_connect_side(&Segments[room], &Segments[neighbor]);
			break;
		}
	check(neighbor >= 0 && neighbor_side >= 0, "Surface controls require a real reciprocal portal");
	const auto configure = [&](int count) {
		std::copy(guard.segments.begin(), guard.segments.end(), Segments);
		std::copy(guard.walls.begin(), guard.walls.end(), Walls);
		for (int segment = 0; segment < Num_segments; ++segment)
			for (auto &face : Segments[segment].sides) face.tmap_num = face.tmap_num2 = 0;
		for (int i = 0; i < NumTextures; ++i) TmapInfo[i].eclip_num = -1;
		for (int i = 0; i < Num_effects; ++i) Effects[i].sound_num = -1;
		TmapInfo[1].eclip_num = 0;
		TmapInfo[2].eclip_num = 1;
		Effects[0].sound_num = 7;
		Effects[1].sound_num = 8;
		Sounds[7] = 0;
		Sounds[8] = 1;
		Sounds[9] = 255;
		Walls[0].type = WALL_ILLUSION;
		Walls[0].flags = 0;
		Walls[0].cloak_value = 31;
		for (int side = 0; side < count; ++side) {
			Segments[room].sides[side].tmap_num2 = 1;
			Segments[room].sides[side].wall_num = 0;
		}
		listener.segnum = short(room);
		compute_segment_center(&listener.pos, &Segments[room]);
		GameCfg.DigiVolume = 8;
		clear_linked_scene();
	};
	const auto initialize = [&] {
		set_sound_sources();
		check(!deferred && !Mix_Playing(-1) && named_audio_snapshot().at("voices").empty(), "Surface initialization started a channel before frame sync or leaked into Redux");
		for (unsigned i = 0; i < owners.count; ++i) check(owners.entries[i].flags == (sound_used | sound_link_position | sound_forever | sound_permanent) && owners.entries[i].channel == -1, "Surface setup lost permanent deferred ownership");
		record(receipt());
	};
	const auto emit = [&] {
		xl_audio_sync_objects();
		check(owners.valid(mixer) && !Mix_Playing(-1) && !named_audio_snapshot().at("voices").empty(), "Surface sources failed to enter the XL pool");
		output();
	};
	for (int count = 1; count <= 6; ++count) {
		configure(count);
		initialize();
		check(owners.count == unsigned(count), "Surface initializer omitted a visible emitting face");
		for (unsigned i = 0; i < owners.count; ++i) check(owners.entries[i].max_volume == F1_0 / (2 * count), "Surface gain was not divided among the segment's emitting sides");
		emit();
	}
	configure(1);
	Segments[room].sides[0].tmap_num = 2;
	Segments[room].sides[0].tmap_num2 = 3;
	initialize();
	check(owners.count == 1 && owners.entries[0].sound == 1, "Overlay without an effect failed to use the base effect");
	emit();
	TmapInfo[3].eclip_num = 2;
	initialize();
	check(!owners.count, "Silent overlay effect incorrectly fell back to base audio");
	configure(2);
	TmapInfo[3].eclip_num = 2;
	Effects[2].sound_num = 9;
	Segments[room].sides[0].tmap_num2 = 3;
	initialize();
	check(owners.count == 1 && owners.entries[0].sound == 0 && owners.entries[0].max_volume == F1_0 / 4, "Unavailable mapped sample incorrectly changed the emitting-side divisor");
	emit();
	configure(2);
	Effects[0].sound_num = SOUND_FORCEFIELD_HUM;
	Sounds[SOUND_FORCEFIELD_HUM] = 0;
	GameCfg.DigiVolume = 1;
	initialize();
	check(owners.count == 2 && owners.entries[0].max_volume == F1_0 / 16 && owners.entries[1].max_volume == F1_0 / 16, "Distributed forcefield hum lost custom scaling or its minimum");
	emit();
	configure(0);
	auto &first = Segments[room].sides[portal_side];
	auto &second = Segments[neighbor].sides[neighbor_side];
	first.wall_num = second.wall_num = 0;
	first.tmap_num2 = 1;
	second.tmap_num2 = short(1 | 0x4000);
	listener.segnum = short((std::min) (room, neighbor));
	compute_segment_center(&listener.pos, &Segments[listener.segnum]);
	initialize();
	check(owners.count == 1 && owners.entries[0].segment == (std::min) (room, neighbor), "Rotated overlay duplicated a transparent-portal surface source");
	emit();
	second.tmap_num2 = 2;
	initialize();
	check(owners.count == 2, "Different portal overlays incorrectly suppressed a source");
	emit();
	second.tmap_num2 = 1;
	Segments[room].children[portal_side] = Segments[neighbor].children[neighbor_side] = -1;
	clear_linked_scene();
	initialize();
	check(owners.count == 2, "Disconnected solid faces incorrectly suppressed a source");
	configure(2);
	initialize();
	emit();
	const auto earned = named_audio_snapshot();
	check(earned.at("voices").size() == 2 && earned.at("voices")[0][3] > 0 && earned.at("voices")[1][3] > 0, "Surface loops did not earn playback positions");
	xl_named_audio_stop();
	check(restore_named_audio(earned, true) && named_audio_snapshot() == earned && owners.valid(mixer), "Surface restore lost gains, ownership or playback phase");
	emit();
	const auto before_restart = receipt().owners;
	initialize();
	check(receipt().owners == before_restart, "Surface reinitialization accumulated or changed owners");
	guard.restore();
	check(restore_named_audio(initial, true), "Cannot restore incoming audio after surface controls");
}
} // namespace xl_surface_audio_control
#endif
