#ifndef DXX_XL_GAMEPLAY_DISPATCH_TEST_H
#define DXX_XL_GAMEPLAY_DISPATCH_TEST_H
#include "gameplay_audio.h"
#include "linked_audio.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "preview.h"
#include <SDL_mixer.h>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "digi.h"
#include "sounds.h"
#include "args.h"
#include "newdemo.h"
#include "physfsx.h"
extern PHYSFS_file *outfile;
void digi_record_sound_objects(void);
}
namespace xl_gameplay_dispatch_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
inline void demo(int source)
{
	constexpr const char *path = "xl-audio-dispatch-control.tmp";
	check(!outfile && Newdemo_state == ND_STATE_NORMAL && !PHYSFS_exists(path), "Audio demo control requires an unused temporary stream");
	struct Guard {
		~Guard()
		{
			Newdemo_state = ND_STATE_NORMAL;
			if (outfile) PHYSFS_close(outfile);
			outfile = nullptr;
			PHYSFS_delete("xl-audio-dispatch-control.tmp");
		}
	} guard;
	Objects[source].pos = Viewer->pos;
	outfile = PHYSFSX_openWriteBuffered(path);
	check(outfile, "Cannot open audio demo control");
	Newdemo_state = ND_STATE_RECORDING;
	digi_link_sound_to_object(7, short(source), 0, F1_0);
	digi_link_sound_to_pos(7, Viewer->segnum, 0, &Viewer->pos, 0, F1_0);
	check(digi_link_sound_to_object(7, short(source), 1, F1_0) >= 0, "Cannot record persistent audio owner");
	digi_link_sound_to_object(7, short(source), 1, F1_0);
	digi_record_sound_objects();
	digi_kill_sound_linked_to_object(source);
	Newdemo_state = ND_STATE_NORMAL;
	check(PHYSFS_close(outfile), "Cannot close audio demo control");
	outfile = nullptr;
	auto *input = PHYSFS_openRead(path);
	check(input, "Cannot read audio demo control");
	std::vector<unsigned char> bytes(std::size_t(PHYSFS_fileLength(input)));
	const auto read = PHYSFS_readBytes(input, bytes.data(), bytes.size());
	check(PHYSFS_close(input) && read == PHYSFS_sint64(bytes.size()), "Cannot read complete audio demo events");
	check(bytes.size() == 81 && bytes[0] == 7 && bytes[13] == 7 && bytes[26] == 49 && bytes[51] == 49 && bytes[76] == 50,
	      "Engine audio demo lost one-shot, persistent, existing-owner or kill events, or recorded a duplicate owner");
	const auto integer = [&](unsigned offset) {
		return unsigned(bytes.at(offset)) | unsigned(bytes.at(offset + 1)) << 8 | unsigned(bytes.at(offset + 2)) << 16 | unsigned(bytes.at(offset + 3)) << 24;
	};
	for (unsigned offset : { 0u, 13u, 26u }) check(integer(offset + 1) == 7, "Audio demo recorded a physical ID instead of the caller's logical sound");
	// Recording an already-live owner uses the source's reverse-table alias
	check(integer(52) < MAX_SOUNDS && digi_xlat_sound(int(integer(52))) == xl_gameplay_sound_sample(7), "Existing audio owner recorded an unrelated logical alias");
	check(integer(9) == F1_0 && integer(22) == F1_0 && integer(31) == unsigned(Objects[source].signature) &&
	          integer(56) == unsigned(Objects[source].signature) && integer(77) == unsigned(Objects[source].signature),
	      "Audio demo lost spatial volume or object lifetime identity");
	digi_stop_digi_sounds();
}
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	const auto initial = named_audio_snapshot();
	struct Guard {
		int low_memory = GameArg.SysLowMem;
		unsigned char a = Sounds[7], b = Sounds[8], c = Sounds[9], alternate = AltSounds[7];
		int object = -1;
		~Guard()
		{
			Sounds[7] = a;
			Sounds[8] = b;
			Sounds[9] = c;
			AltSounds[7] = alternate;
			GameArg.SysLowMem = low_memory;
			if (object >= 0) obj_delete(object);
		}
	} guard;
	Sounds[7] = 0;
	Sounds[8] = 1;
	Sounds[9] = 255;
	AltSounds[7] = 8;
	GameArg.SysLowMem = 0;
	SDL_PauseAudio(1);
	digi_init_sounds();
	const auto clean = named_audio_snapshot();
	check(clean.at("voices").empty() && clean.at("linked").at("owners").empty(), "Engine sound initialization retained XL playback");
	const auto reset = [&] {
		check(restore_named_audio(clean, true), "Cannot reset engine sound dispatch control");
	};
	const auto audible = [&] {
		check(Mix_Playing(-1) == 0, "XL engine sound also entered the Redux mixer");
		check(named_audio_snapshot().at("voices").size() == 1, "Engine sound did not produce exactly one XL voice");
		output();
	};
	digi_play_sample(7, F1_0);
	const auto primary_sample = named_audio_snapshot().at("voices")[0][2];
	audible();
	reset();
	digi_play_sample_once(7, F1_0);
	audible();
	reset();
	digi_play_sample_3d(-1, 65536, F1_0, 0);
	const auto alternate_sample = named_audio_snapshot().at("voices")[0][2];
	audible();
	reset();
	// XL accepts positive volumes below Redux's historical 3D cutoff
	digi_play_sample_3d(7, 0, 9, 1);
	audible();
	reset();
	GameArg.SysLowMem = 1;
	digi_play_sample(7, F1_0);
	check(named_audio_snapshot().at("voices")[0][2] == alternate_sample && alternate_sample != primary_sample, "Low-memory dispatch did not select the alternate sample");
	audible();
	reset();
	GameArg.SysLowMem = 0;
	digi_play_sample(9, F1_0);
	digi_play_sample(7, 0);
	digi_play_sample_3d(INT32_MIN, 0, F1_0, 0);
	check(named_audio_snapshot() == clean && Mix_Playing(-1) == 0, "Rejected engine request changed playback or fell through to Redux");
	guard.object = obj_create(OBJ_MARKER, 0, Viewer->segnum, &Viewer->pos, &Viewer->orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	check(guard.object >= 0, "Cannot create engine audio source");
	const int source = guard.object;
	auto position = Viewer->pos;
	check(digi_link_sound_to_object(7, short(source), 0, F1_0) == -1, "One-shot engine object unexpectedly retained an owner");
	audible();
	check(named_audio_snapshot().at("linked").at("owners").empty(), "One-shot engine object retained a registry entry");
	reset();
	check(digi_link_sound_to_pos(7, Viewer->segnum, 0, &position, 0, F1_0) == -1, "One-shot engine position unexpectedly retained an owner");
	audible();
	reset();
	const int token = digi_link_sound_to_object3(7, short(source), 1, F1_0, 64 * F1_0, -1, -1);
	check(token >= 0 && digi_link_sound_to_object3(7, short(source), 1, F1_0, 64 * F1_0, -1, -1) == token &&
	          xl_linked_sound_count(source, 7, 0) == 1 && xl_linked_sound_count(source, 7, 1) == 1,
	      "Engine object dispatch lost duplicate ownership or registry queries");
	const auto before_move = named_audio_snapshot().at("linked").at("owners")[0];
	vm_vec_scale_add2(&Objects[source].pos, &Viewer->orient.rvec, 10 * F1_0);
	digi_sync_sounds();
	const auto moved = named_audio_snapshot().at("linked").at("owners")[0];
	check(moved.at("serial") == before_move.at("serial") && moved.at("pan") != before_move.at("pan"), "Engine synchronization did not move its XL owner");
	audible();
	digi_kill_sound_linked_to_object(source);
	check(!xl_linked_sound_count(source, 7, 0) && named_audio_snapshot().at("voices").empty(), "Engine destroy left XL object playback alive");
	reset();
	xl_audio_defer_objects(1);
	check(digi_link_sound_to_pos2(7, Viewer->segnum, 2, &position, 1, F1_0, 64 * F1_0) >= 0 &&
	          xl_segment_sound_count(Viewer->segnum, 2, 7, 0) == 1 && !xl_segment_sound_count(Viewer->segnum, 2, 7, 1),
	      "Deferred engine segment played early or lost its owner");
	xl_audio_defer_objects(0);
	digi_sync_sounds();
	check(xl_segment_sound_count(Viewer->segnum, 2, 7, 1) == 1, "Deferred engine segment did not start at synchronization");
	audible();
	digi_kill_sound_linked_to_segment(Viewer->segnum, 2, 8);
	check(xl_segment_sound_count(Viewer->segnum, 2, 7, 1) == 1, "Engine segment kill removed a different sound");
	digi_kill_sound_linked_to_segment(Viewer->segnum, 2, 7);
	check(!xl_segment_sound_count(Viewer->segnum, 2, 7, 0) && named_audio_snapshot().at("voices").empty(), "Engine segment kill retained playback");
	digi_play_sample(7, F1_0);
	digi_stop_digi_sounds();
	check(named_audio_snapshot().at("voices").empty() && Mix_Playing(-1) == 0, "Engine stop did not clear both sound runtimes");
	demo(source);
	check(restore_named_audio(initial, true), "Cannot restore state after engine dispatch controls");
}
} // namespace xl_gameplay_dispatch_control
#endif
