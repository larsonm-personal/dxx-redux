#ifndef DXX_XL_EXTERNAL_AUDIO_TEST_H
#define DXX_XL_EXTERNAL_AUDIO_TEST_H
#include "surface_audio_test.h"
namespace xl_external_audio_control
{
using xl_surface_audio_control::check;
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	SDL_PauseAudio(1);
	const auto initial = named_audio_snapshot();
	object listener = *Viewer;
	listener.orient = vmd_identity_matrix;
	auto position = listener.pos;
	position.x += 10 * F1_0;
	position.z += 10 * F1_0;
	const int source = obj_create(OBJ_MARKER, 0, listener.segnum, &position, &listener.orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	check(source >= 0, "Cannot create external audio lifetime control");
	struct Guard {
		int source, gain = priority_volume;
		object *viewer = Viewer;
		void finish()
		{
			if (source < 0) return;
			SDL_PauseAudio(1);
			xl_named_audio_stop();
			xl_audio_defer_objects(0);
			obj_delete(source);
			Viewer = viewer;
			xl_named_audio_set_volume(gain);
			source = -1;
		}
		~Guard()
		{
			finish();
		}
	} guard{ source };
	Viewer = &listener;
	xl_named_audio_stop();
	xl_audio_defer_objects(0);
	xl_named_audio_set_volume(32768);
	const auto create = [&](const char *name, int forever = 1) {
		return xl_audio_link_external(name, 1, source, forever, F1_0, 64 * F1_0, 0, 0, 0, 0);
	};
	const auto find = [&](std::int64_t serial) {
		for (unsigned i = 0; i < owners.count; ++i)
			if (owners.entries[i].serial == std::uint64_t(serial)) return owners.entries[i];
		throw std::runtime_error("External sound lost its owner identity");
	};
	const auto empty = named_audio_snapshot();
	for (const char *invalid : { "missing.wav", "../sound.wav", "" })
		check(create(invalid) == -1 && named_audio_snapshot() == empty, "Unprepared external sound changed live playback");
	check(create("mono.wav", 0) == -1 && owners.count == 0 && named_audio_snapshot().at("voices").size() == 1, "External one-shot became a persistent owner or stayed silent");
	output();
	const auto one_shot = named_audio_snapshot();
	check(restore_named_audio(one_shot, false) && restore_named_audio(one_shot, true) && named_audio_snapshot() == one_shot, "External one-shot phase did not restore");
	xl_named_audio_stop();
	const auto ordinary = xl_audio_link_object(0, 3, source, 1, F1_0, 64 * F1_0, -1, -1, 0, 0);
	const auto sound = create("sound.wav"), mono = create("mono.wav");
	check(ordinary > 0 && sound > ordinary && mono > sound && owners.count == 3 && owners.active() == 3, "External resource aliases an ordinary sample or another WAV");
	check(find(sound).external && find(sound).sound == -1 && find(sound).sample == unsigned(external_sample("sound.wav")) && find(sound).decay == 1 && find(sound).ambient, "External creation lost its resource, source decay or ambient class");
	check(create("SOUND.WAV") == sound && create("mono.wav") == mono && owners.count == 3, "External duplicate lookup added another live owner");
	output();
	const auto earned = named_audio_snapshot();
	for (unsigned variant = 0; variant < 8; ++variant) {
		auto bad = earned;
		auto &row = bad["linked"]["owners"][1];
		if (variant == 0) row["external"] = false;
		if (variant == 1) row["sound"] = 0;
		if (variant == 2) row["sample"] = classic_sample(0);
		if (variant == 3) row["sample"] = 99999;
		if (variant == 4) row["sample"] = external_sample("mono.wav");
		if (variant == 5) row["signature"] = row["signature"].get<int>() + 1;
		if (variant == 6) row["channel_serial"] = bad["next"];
		if (variant == 7) bad["source"] = "foreign resources";
		check(!restore_named_audio(bad, false) && !restore_named_audio(bad, true) && named_audio_snapshot() == earned, "Malformed external resource binding was accepted or changed playback");
	}
	xl_named_audio_stop();
	check(restore_named_audio(earned, true) && named_audio_snapshot() == earned, "External owners did not recover earned playback phase");
	output();
	const auto before_move = find(sound);
	Objects[source].pos.x -= 20 * F1_0;
	xl_audio_sync_objects();
	const auto moved = find(sound);
	if (moved.serial != before_move.serial || moved.channel != before_move.channel || moved.pan == before_move.pan)
		throw std::runtime_error("Moving external source differs: channel " + std::to_string(before_move.channel) + "/" + std::to_string(moved.channel) + ", pan " + std::to_string(before_move.pan) + "/" + std::to_string(moved.pan) + ", volume " + std::to_string(before_move.volume) + "/" + std::to_string(moved.volume));
	output();
	Objects[source].pos.x = listener.pos.x + 500 * F1_0;
	xl_audio_sync_objects();
	check(owners.count == 3 && !owners.active() && create("sound.wav") == sound && owners.count == 3, "Inaudible external duplicate accumulated or deleted its owner");
	const auto stopped = named_audio_snapshot();
	check(restore_named_audio(stopped, true) && named_audio_snapshot() == stopped, "Stopped external owners did not restore");
	Objects[source].pos = position;
	xl_audio_sync_objects();
	check(owners.active() == 3 && find(sound).serial == std::uint64_t(sound), "External owner failed to restart after range return");
	output();
	check(xl_audio_destroy_object(source, 0) && owners.count == 2 && find(sound).external && find(mono).external, "Ordinary-sample destruction removed an external owner");
	output();
	const int companion = xl_audio_start_sample(1, 2, F1_0, 32767, 1, -1, -1);
	check(companion >= 0, "Cannot create external lifetime companion");
	++Objects[source].signature;
	xl_audio_sync_objects();
	check(!owners.count && named_audio_snapshot().at("voices").size() == 1 && xl_audio_channel_playing(companion), "Object reuse retained external playback or stopped an unrelated voice");
	output();
	xl_named_audio_stop();
	xl_audio_defer_objects(1);
	const auto deferred_owner = create("sound.wav");
	check(deferred_owner > 0 && find(deferred_owner).channel < 0 && (find(deferred_owner).flags & sound_permanent) && create("sound.wav") == deferred_owner && owners.count == 1, "Deferred external duplicate lost its permanent owner");
	xl_audio_sync_objects();
	check(!owners.active(), "Deferred external owner started too soon");
	xl_audio_defer_objects(0);
	xl_audio_sync_objects();
	check(owners.active() == 1, "Deferred external owner did not start");
	output();
	guard.finish();
	check(restore_named_audio(initial, true), "Cannot restore audio state after external controls");
}
} // namespace xl_external_audio_control
#endif
