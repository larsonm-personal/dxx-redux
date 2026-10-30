#ifndef DXX_XL_COMBAT_AUDIO_TEST_H
#define DXX_XL_COMBAT_AUDIO_TEST_H
#include "gameplay_audio.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "shared_audio_internal.h"
#include <SDL_mixer.h>
#include <array>
#include <stdexcept>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
#include "fireball.h"
#include "sounds.h"
#include "args.h"
#include "playsave.h"
#include "rng_shared.h"
#include "bm.h"
#include "collide.h"
#include "newdemo.h"
#include "physfsx.h"
extern PHYSFS_file *outfile;
object *explode_badass_object(object *actor, fix damage, fix distance, fix force);
void collide_weapon_and_wall(object *weapon, fix speed, short segment, short side, vms_vector *position);
}
namespace xl_combat_audio_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	const auto initial = named_audio_snapshot();
	struct Guard {
		object *viewer = Viewer;
		std::vector<int> signatures;
		std::vector<weapon_info> weapons;
		int low_memory = GameArg.SysLowMem;
		decltype(PlayerCfg.QuietPlasma) quiet = PlayerCfg.QuietPlasma;
		unsigned char sound7 = Sounds[7], sound22 = Sounds[22];
		unsigned sim = 0, fx = 0, sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		Guard() : weapons(Weapon_info, Weapon_info + N_weapon_types)
		{
			for (int i = 0; i <= Highest_object_index; ++i) signatures.push_back(Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature);
			check(d_rand_get_stream_state(D_RNG_SIM, &sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "Combat audio requires stateful RNG streams");
		}
		~Guard()
		{
			Viewer = viewer;
			for (int i = Highest_object_index; i >= 0; --i)
				if (Objects[i].type != OBJ_NONE && (unsigned(i) >= signatures.size() || signatures[unsigned(i)] != Objects[i].signature)) obj_delete(i);
			std::copy(weapons.begin(), weapons.end(), Weapon_info);
			GameArg.SysLowMem = low_memory;
			PlayerCfg.QuietPlasma = quiet;
			Sounds[7] = sound7;
			Sounds[22] = sound22;
			d_rand_set_stream_state(D_RNG_SIM, sim);
			d_rand_set_stream_state(D_RNG_FX, fx);
			d_rand_set_stream_call_count(D_RNG_SIM, sim_calls);
			d_rand_set_stream_call_count(D_RNG_FX, fx_calls);
		}
	} guard;
	// The spatial fixture can use a detached listener; firing requires an engine object
	Viewer = ConsoleObject;
	SDL_PauseAudio(1);
	xl_named_audio_stop();
	const auto clean = named_audio_snapshot();
	GameArg.SysLowMem = 0;
	PlayerCfg.QuietPlasma = 1;
	Sounds[7] = 0;
	Sounds[22] = 1;
	const auto reset = [&] { check(restore_named_audio(clean, true), "Cannot reset combat audio control"); };
	const auto audible = [&](int sound, int sound_class, int volume) {
		const auto state = named_audio_snapshot();
		check(Mix_Playing(-1) == 0 && state.at("voices").size() == 1, "Combat event duplicated Redux audio or failed to create one XL voice");
		const auto &voice = state.at("voices")[0];
		if (voice[2] != classic_sample(Sounds[sound]) || voice[8] != sound_class || voice[6] != std::int64_t(volume) * priority_volume / F1_0)
			throw std::runtime_error("Combat event lost sample/class/volume: logical=" + std::to_string(sound) + " sample=" + std::to_string(classic_sample(Sounds[sound])) + " class=" + std::to_string(sound_class) + " volume=" + std::to_string(volume) + " gain=" + std::to_string(priority_volume) + " actual=" + voice.dump());
		output();
	};
	const int remote = obj_create(OBJ_MARKER, 0, Viewer->segnum, &Viewer->pos, &Viewer->orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	check(remote >= 0, "Cannot create remote combat audio parent");
	for (int id : std::array<int, 4>{ LASER_ID, VULCAN_ID, PLASMA_ID, 6 }) {
		Weapon_info[id].flash_sound = id == 6 ? 22 : 7;
		for (int parent : { int(Viewer - Objects), remote }) {
			reset();
			const int shot = Laser_create_new(&Viewer->orient.fvec, &Viewer->pos, Viewer->segnum, parent, id, 1);
			check(shot >= 0, "Actual weapon creation failed in combat audio control");
			const bool local = parent == Viewer - Objects;
			audible(id == 6 ? 22 : 7, local ? XL_SOUND_PLAYER : XL_SOUND_GENERIC, local && id == VULCAN_ID ? F1_0 / 2 : F1_0);
		}
	}
	reset();
	Weapon_info[LASER_ID].flash_sound = 7;
	check(Laser_create_new(&Viewer->orient.fvec, &Viewer->pos, Viewer->segnum, int(Viewer - Objects), LASER_ID, 0) >= 0 && named_audio_snapshot() == clean, "Silent actual firing changed audio");
	Weapon_info[LASER_ID].flash_sound = -1;
	check(Laser_create_new(&Viewer->orient.fvec, &Viewer->pos, Viewer->segnum, int(Viewer - Objects), LASER_ID, 1) >= 0 && named_audio_snapshot() == clean, "Missing firing cue changed audio");
	Weapon_info[LASER_ID].flash_sound = 7;
	Sounds[7] = 255;
	check(Laser_create_new(&Viewer->orient.fvec, &Viewer->pos, Viewer->segnum, int(Viewer - Objects), LASER_ID, 1) >= 0 && named_audio_snapshot() == clean && !Mix_Playing(-1), "Unavailable XL firing cue fell through to Redux");
	Sounds[7] = 0;
	// A stale/missing parent is permitted by the source sound consumer
	object orphan{};
	orphan.type = OBJ_WEAPON;
	orphan.ctype.laser_info.parent_num = -1;
	for (int id : { LASER_ID, VULCAN_ID }) {
		reset();
		orphan.id = static_cast<unsigned char>(id);
		check(xl_gameplay_weapon_fire(&orphan, 1), "XL orphan firing was not consumed");
		audible(7, XL_SOUND_LASER, id == VULCAN_ID ? F1_0 / 2 : F1_0);
	}
	reset();
	orphan.id = LASER_ID;
	orphan.ctype.laser_info.parent_num = remote;
	orphan.segnum = Viewer->segnum;
	orphan.pos = Viewer->pos;
	{
		const auto type = Objects[remote].type;
		Objects[remote].type = OBJ_NONE;
		const int handled = xl_gameplay_weapon_fire(&orphan, 1);
		Objects[remote].type = type;
		check(handled, "XL empty parent slot was not consumed");
		audible(7, XL_SOUND_GENERIC, F1_0);
	}
	check(Sounds[11] != Sounds[57] && Sounds[11] != Sounds[58] && Sounds[57] != Sounds[58], "Explosion controls require three distinct real bank samples");
	for (int id : { CONCUSSION_ID, MEGA_ID }) {
		reset();
		const int shot = obj_create(OBJ_WEAPON, id, Viewer->segnum, &Viewer->pos, &Viewer->orient, F1_0, CT_WEAPON, MT_NONE, RT_NONE);
		check(shot >= 0, "Cannot create explosion audio weapon");
		Objects[shot].ctype.laser_info.parent_num = int(Viewer - Objects);
		Objects[shot].ctype.laser_info.parent_type = Viewer->type;
		// Isolate the audio event while executing the real explosion creation
		Weapon_info[id].strength[Difficulty_level] = 0;
		Weapon_info[id].damage_radius = F1_0;
		Weapon_info[id].children = -1;
		check(explode_badass_weapon(&Objects[shot], &Objects[shot].pos), "Actual weapon explosion failed");
		audible(id == MEGA_ID ? 57 : 11, XL_SOUND_EXPLOSION, F1_0);
	}
	reset();
	check(explode_badass_object(&Objects[remote], 0, 0, 0), "Actual actor explosion failed");
	audible(58, XL_SOUND_EXPLOSION, F1_0);
	reset();
	{
		auto &face = Segments[Viewer->segnum].sides[0];
		struct WaterGuard {
			side &face;
			side saved;
			decltype(TmapInfo[0].flags) flags;
			~WaterGuard()
			{
				face = saved;
				TmapInfo[0].flags = flags;
			}
		} water{ face, face, TmapInfo[0].flags };
		face.wall_num = -1;
		face.tmap_num = face.tmap_num2 = 0;
		TmapInfo[0].flags = TMI_WATER;
		const int shot = obj_create(OBJ_WEAPON, MEGA_ID, Viewer->segnum, &Viewer->pos, &Viewer->orient, F1_0, CT_WEAPON, MT_NONE, RT_NONE);
		check(shot >= 0, "Cannot create water impact weapon");
		auto &weapon = Objects[shot];
		weapon.ctype.laser_info.parent_num = int(Viewer - Objects);
		weapon.ctype.laser_info.parent_type = Viewer->type;
		weapon.ctype.laser_info.parent_signature = Viewer->signature;
		weapon.mtype.phys_info.velocity = Viewer->orient.fvec;
		weapon.flags |= OF_SILENT; // Suppress unrelated AI awareness, not the water cues
		Weapon_info[MEGA_ID].matter = 1;
		collide_weapon_and_wall(&weapon, F1_0, Viewer->segnum, 0, &weapon.pos);
		const auto state = named_audio_snapshot();
		check((weapon.flags & OF_SHOULD_BE_DEAD) && state.at("voices").size() == 2 && !Mix_Playing(-1), "Actual water impact did not emit its two XL cues");
		unsigned impact = 0, explosion = 0;
		for (const auto &voice : state.at("voices")) {
			impact += voice[2] == classic_sample(Sounds[SOUND_MISSILE_HIT_WATER]) && voice[8] == XL_SOUND_GENERIC;
			explosion += voice[2] == classic_sample(Sounds[57]) && voice[8] == XL_SOUND_EXPLOSION;
		}
		check(impact == 1 && explosion == 1, "Water impact selected the wrong explosion sample or class");
		output();
	}
	reset();
	{
		constexpr const char *path = "xl-combat-audio-control.tmp";
		check(!outfile && Newdemo_state == ND_STATE_NORMAL && !PHYSFS_exists(path), "Combat demo control requires an unused temporary stream");
		struct DemoGuard {
			~DemoGuard()
			{
				Newdemo_state = ND_STATE_NORMAL;
				if (outfile) PHYSFS_close(outfile);
				outfile = nullptr;
				PHYSFS_delete("xl-combat-audio-control.tmp");
			}
		} demo;
		outfile = PHYSFSX_openWriteBuffered(path);
		check(outfile, "Cannot open combat demo control");
		Newdemo_state = ND_STATE_RECORDING;
		check(Laser_create_new(&Viewer->orient.fvec, &Viewer->pos, Viewer->segnum, int(Viewer - Objects), VULCAN_ID, 1) >= 0, "Cannot record actual weapon firing");
		Newdemo_state = ND_STATE_NORMAL;
		check(PHYSFS_close(outfile), "Cannot close combat demo stream");
		outfile = nullptr;
		auto *input = PHYSFS_openRead(path);
		check(input, "Cannot read combat demo control");
		std::array<unsigned char, 13> bytes{};
		const bool size = PHYSFS_fileLength(input) == PHYSFS_sint64(bytes.size());
		const auto read = PHYSFS_readBytes(input, bytes.data(), bytes.size());
		check(PHYSFS_close(input) && size && read == PHYSFS_sint64(bytes.size()), "Combat demo lost or duplicated its local firing event");
		const auto integer = [&](unsigned offset) { return unsigned(bytes[offset]) | unsigned(bytes[offset + 1]) << 8 | unsigned(bytes[offset + 2]) << 16 | unsigned(bytes[offset + 3]) << 24; };
		check(bytes[0] == 7 && integer(1) == 7 && integer(5) == 32767 && integer(9) == F1_0 / 2, "Combat demo lost logical sound, centered pan or Vulcan volume");
	}
	check(restore_named_audio(initial, true), "Cannot restore incoming audio after combat controls");
}
} // namespace xl_combat_audio_control
#endif
