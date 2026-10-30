#ifndef DXX_XL_AFTERBURNER_AUDIO_TEST_H
#define DXX_XL_AFTERBURNER_AUDIO_TEST_H
#include "gameplay_audio.h"
#include "linked_audio.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "shared_audio_internal.h"
#include "trigger_state.h"
#include <SDL_mixer.h>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "kconfig.h"
#include "endlevel.h"
#include "sounds.h"
#include "digi.h"
#include "args.h"
#include "physfsx.h"
#include "newdemo.h"
extern PHYSFS_file *outfile;
extern ubyte Last_afterburner_state;
extern fix Last_afterburner_charge;
void do_afterburner_stuff(void);
}
namespace xl_afterburner_audio_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
struct Guard {
	object *viewer = Viewer;
	decltype(Players[Player_num].flags) flags = Players[Player_num].flags;
	fix charge = Players[Player_num].afterburner_charge, last_charge = Last_afterburner_charge;
	decltype(Controls.afterburner_state) held = Controls.afterburner_state;
	ubyte last = Last_afterburner_state;
	int dead = Player_is_dead, end = Endlevel_sequence, rate = GameArg.SndDigiSampleRate, low_memory = GameArg.SysLowMem;
	unsigned char ignite = Sounds[SOUND_AFTERBURNER_IGNITE];
	~Guard()
	{
		Viewer = viewer;
		Players[Player_num].flags = flags;
		Players[Player_num].afterburner_charge = charge;
		Last_afterburner_charge = last_charge;
		Controls.afterburner_state = held;
		Last_afterburner_state = last;
		Player_is_dead = dead;
		Endlevel_sequence = end;
		GameArg.SndDigiSampleRate = rate;
		GameArg.SysLowMem = low_memory;
		Sounds[SOUND_AFTERBURNER_IGNITE] = ignite;
	}
};
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	Guard guard;
	SDL_PauseAudio(1);
	const auto initial = named_audio_snapshot();
	const int volume = priority_volume;
	Viewer = ConsoleObject;
	GameArg.SysLowMem = 0;
	Player_is_dead = Endlevel_sequence = 0;
	auto &player = Players[Player_num];
	player.flags |= PLAYER_FLAGS_AFTERBURNER;
	player.afterburner_charge = F1_0;
	Controls.afterburner_state = 1;
	Last_afterburner_state = 0;
	Last_afterburner_charge = 0;
	xl_named_audio_stop();
	digi_set_digi_volume(32768);
	const auto clean = named_audio_snapshot();
	const int sample = xl_gameplay_sound_sample(SOUND_AFTERBURNER_IGNITE);
	check(sample >= 0 && sample != xl_gameplay_sound_sample(SOUND_AFTERBURNER_PLAY), "Afterburner controls require distinct real ignite/shutdown samples");
	const auto loop = [&] {
		check(owners.count == 1 && owners.entries[0].sound == sample && owners.entries[0].object == player.objnum &&
		          owners.entries[0].signature == Objects[player.objnum].signature && owners.entries[0].sound_class == XL_SOUND_PLAYER &&
		          owners.entries[0].decay == XL_SOUND_PLAYER && owners.entries[0].max_volume == F1_0 && owners.entries[0].max_distance == 256 * F1_0 &&
		          (owners.entries[0].flags & sound_forever) && owners.valid(mixer) && !Mix_Playing(-1),
		      "Afterburner lost its single classed owner or leaked into Redux");
	};
	for (int rate : { 11025, 22050, 44100 }) {
		check(restore_named_audio(clean, true), "Cannot reset afterburner rate control");
		GameArg.SndDigiSampleRate = rate;
		do_afterburner_stuff();
		loop();
		check(owners.entries[0].loop_start == 16013 && owners.entries[0].loop_end == 24226, "Redux bank rate changed the XL S16-profile loop boundaries");
		const auto started = named_audio_snapshot();
		for (int i = 0; i < 64; ++i) do_afterburner_stuff();
		check(named_audio_snapshot() == started && Last_afterburner_state == 1 && Last_afterburner_charge == F1_0, "Held afterburner duplicated or restarted its loop");
		output();
	}
	const auto earned = named_audio_snapshot();
	check(earned.at("voices").size() == 1 && earned.at("voices")[0][3] > 0, "Afterburner did not earn a playback position");
	constexpr const char *path = "xl-afterburner-state.tmp";
	check(!PHYSFS_exists(path), "Afterburner trailer control requires an unused temporary path");
	struct FileGuard {
		~FileGuard()
		{
			PHYSFS_delete("xl-afterburner-state.tmp");
		}
	} file_guard;
	auto *file = PHYSFS_openWrite(path);
	REWIND_PHYSFS_FILE(writer, file);
	check(file && xl_wall_trigger_write(writer) && PHYSFS_close(file), "Cannot write afterburner runtime trailer");
	xl_named_audio_stop();
	Last_afterburner_state = 0;
	Last_afterburner_charge = 0;
	file = PHYSFS_openRead(path);
	REWIND_PHYSFS_FILE(reader, file);
	check(file && xl_wall_trigger_read(reader) && PHYSFS_close(file) && named_audio_snapshot() == earned, "Afterburner trailer lost ownership or earned phase");
	do_afterburner_stuff();
	check(named_audio_snapshot() == earned, "First controller call after restore restarted the afterburner");
	output();
	xl_audio_destroy_object(player.objnum, sample);
	do_afterburner_stuff();
	loop();
	check(owners.entries[0].serial != earned.at("linked").at("owners")[0].at("serial"), "Missing afterburner did not acquire a new identity");
	output();
	const auto serial = owners.entries[0].serial;
	{
		AudioLock lock;
		mixer.stop(owners.entries[0].channel);
	}
	for (int i = 0; i < 64; ++i) do_afterburner_stuff();
	check(owners.count == 1 && owners.entries[0].serial == serial && owners.entries[0].channel < 0, "Stopped afterburner accumulated duplicate owners");
	xl_audio_sync_objects();
	loop();
	check(owners.entries[0].channel >= 0, "Stopped afterburner failed to resume through frame sync");
	output();
	const int other_sample = xl_gameplay_sound_sample(SOUND_SHIP_IN_WATER);
	check(other_sample >= 0 && other_sample != sample && xl_gameplay_sound_object(SOUND_SHIP_IN_WATER, XL_SOUND_PLAYER, player.objnum, 1, F1_0 / 2, 256 * F1_0, -1, -1, 0) > 0,
	      "Cannot create unrelated player loop");
	Controls.afterburner_state = 0;
	do_afterburner_stuff();
	const auto released = named_audio_snapshot();
	check(!xl_audio_object_count(player.objnum, sample, 0) && xl_audio_object_count(player.objnum, other_sample, 0) == 1 && released.at("voices").size() == 2,
	      "Afterburner release removed an unrelated loop or omitted shutdown");
	unsigned shutdown = 0;
	for (const auto &voice : released.at("voices")) shutdown += voice[2] == classic_sample(xl_gameplay_sound_sample(SOUND_AFTERBURNER_PLAY)) && voice[8] == XL_SOUND_PLAYER;
	check(shutdown == 1, "Afterburner release lost its player-class shutdown cue");
	do_afterburner_stuff();
	check(named_audio_snapshot() == released, "Released key repeated the shutdown cue");
	output();
	check(restore_named_audio(clean, true), "Cannot reset muted afterburner control");
	digi_set_digi_volume(0);
	Controls.afterburner_state = 1;
	for (int i = 0; i < 64; ++i) do_afterburner_stuff();
	loop();
	const auto silent = named_audio_snapshot();
	check(silent.at("voices").size() == 1 && silent.at("voices")[0][10] == 0, "Muted afterburner retained audible gain");
	// The source permits a silent channel at zero effects gain; also exercise
	// losing that channel while muted, without losing the persistent owner
	{
		AudioLock lock;
		mixer.stop(owners.entries[0].channel);
	}
	for (int i = 0; i < 64; ++i) do_afterburner_stuff();
	loop();
	check(owners.entries[0].channel < 0 && named_audio_snapshot().at("voices").empty(), "Stopped muted afterburner accumulated a new voice");
	const auto muted = named_audio_snapshot();
	check(restore_named_audio(muted, true), "Cannot restore stopped afterburner owner");
	Controls.afterburner_state = 0;
	do_afterburner_stuff();
	check(!owners.count && named_audio_snapshot().at("voices").empty(), "Muted release retained a loop or emitted shutdown");
	digi_set_digi_volume(32768);
	xl_audio_sync_objects();
	do_afterburner_stuff();
	check(!owners.count && named_audio_snapshot().at("voices").empty(), "Unmuting resurrected a released afterburner");
	Controls.afterburner_state = 1;
	do_afterburner_stuff();
	player.afterburner_charge = 0;
	do_afterburner_stuff();
	check(!owners.count && named_audio_snapshot().at("voices").size() == 1, "Depleted afterburner did not stop and emit shutdown");
	output();
	for (int terminal = 0; terminal < 2; ++terminal) {
		check(restore_named_audio(clean, true), "Cannot reset terminal afterburner control");
		player.afterburner_charge = F1_0;
		do_afterburner_stuff();
		if (terminal) Endlevel_sequence = 1;
		else Player_is_dead = 1;
		do_afterburner_stuff();
		do_afterburner_stuff();
		check(!owners.count && named_audio_snapshot().at("voices").empty(), "Dead/endlevel player retained or restarted afterburner audio");
		Player_is_dead = Endlevel_sequence = 0;
	}
	player.flags &= ~PLAYER_FLAGS_AFTERBURNER;
	player.afterburner_charge = F1_0;
	do_afterburner_stuff();
	check(player.afterburner_charge == 0 && !owners.count, "Unequipped afterburner retained charge or playback");
	player.flags |= PLAYER_FLAGS_AFTERBURNER;
	player.afterburner_charge = F1_0;
	Sounds[SOUND_AFTERBURNER_IGNITE] = 255;
	do_afterburner_stuff();
	check(!owners.count && named_audio_snapshot().at("voices").empty() && !Mix_Playing(-1), "Missing XL afterburner cue fell through to Redux");
	Sounds[SOUND_AFTERBURNER_IGNITE] = guard.ignite;
	{
		constexpr const char *demo_path = "xl-afterburner-demo.tmp";
		check(!outfile && Newdemo_state == ND_STATE_NORMAL && !PHYSFS_exists(demo_path), "Afterburner demo control requires an unused stream");
		struct DemoGuard {
			~DemoGuard()
			{
				Newdemo_state = ND_STATE_NORMAL;
				if (outfile) PHYSFS_close(outfile);
				outfile = nullptr;
				PHYSFS_delete("xl-afterburner-demo.tmp");
			}
		} demo;
		outfile = PHYSFSX_openWriteBuffered(demo_path);
		check(outfile, "Cannot open afterburner demo control");
		Newdemo_state = ND_STATE_RECORDING;
		do_afterburner_stuff();
		do_afterburner_stuff();
		Controls.afterburner_state = 0;
		do_afterburner_stuff();
		Newdemo_state = ND_STATE_NORMAL;
		check(PHYSFS_close(outfile), "Cannot close afterburner demo control");
		outfile = nullptr;
		auto *input = PHYSFS_openRead(demo_path);
		check(input, "Cannot read afterburner demo control");
		std::array<unsigned char, 43> bytes{};
		const bool size = PHYSFS_fileLength(input) == PHYSFS_sint64(bytes.size());
		const auto read = PHYSFS_readBytes(input, bytes.data(), bytes.size());
		check(PHYSFS_close(input) && size && read == PHYSFS_sint64(bytes.size()), "Afterburner demo lost or duplicated create/kill/shutdown events");
		const auto integer = [&](unsigned offset) { return unsigned(bytes[offset]) | unsigned(bytes[offset + 1]) << 8 | unsigned(bytes[offset + 2]) << 16 | unsigned(bytes[offset + 3]) << 24; };
		check(bytes[0] == 49 && bytes[25] == 50 && bytes[30] == 7 && integer(1) == SOUND_AFTERBURNER_IGNITE && integer(31) == SOUND_AFTERBURNER_PLAY &&
		          integer(5) == unsigned(Objects[player.objnum].signature) && integer(26) == unsigned(Objects[player.objnum].signature) && integer(9) == F1_0 && integer(13) == 256 * F1_0 &&
		          integer(17) == 16013 && integer(21) == 24226 && integer(39) == F1_0,
		      "Afterburner demo changed logical IDs, owner identity, loop bounds or gain");
	}
	digi_set_digi_volume(volume);
	check(restore_named_audio(initial, true), "Cannot restore incoming afterburner audio state");
}
} // namespace xl_afterburner_audio_control
#endif
