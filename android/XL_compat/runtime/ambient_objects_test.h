#ifndef DXX_XL_AMBIENT_OBJECTS_TEST_H
#define DXX_XL_AMBIENT_OBJECTS_TEST_H
#include "ambient_objects.h"
#include "surface_audio_test.h"
#include "trigger_state.h"
extern "C" {
#include "switch.h"
#include "fvi.h"
}
namespace xl_ambient_objects_control
{
using xl_surface_audio_control::check;
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	SDL_PauseAudio(1);
	const auto initial = ambient_objects_snapshot();
	std::string native_state;
	for (int i = 49; i <= 51 && i <= Highest_object_index; ++i)
		native_state += "; " + std::to_string(i) + ":" + std::to_string(Objects[i].type) + "/" + std::to_string(Objects[i].id) + "/" + std::to_string(Objects[i].signature);
	if (initial.size() != 3) throw std::runtime_error("Ambient fixture state: " + initial.dump() + "; native object count " + std::to_string(Highest_object_index + 1));
	check(initial.size() == 3 && initial[0][0] == 49 && initial[1][0] == 50 && initial[2][0] == 51 &&
	          initial[0][2].get<bool>() && !initial[1][2].get<bool>() && initial[2][2].get<bool>(),
	      ("Loaded ambient fixture lost authored objects or initial enabled states: " + initial.dump() + native_state).c_str());
	for (int index : { 49, 50, 51 })
		check(Objects[index].type == 20 && Objects[index].id == 2 && Objects[index].control_type == CT_NONE &&
		          Objects[index].movement_type == MT_NONE && Objects[index].render_type == RT_NONE,
		      "Ambient object native shadow lost normalized identity or became active geometry");
	check(wall_trigger_index(Walls[0].controlling_trigger) == 7 && Triggers[7].num_links == 2,
	      "Mixed effect/wall target list lost its source wall controller or object target");
	object listener = *ConsoleObject;
	listener.orient = vmd_identity_matrix;
	struct Guard {
		object *viewer = Viewer;
		~Guard()
		{
			Viewer = viewer;
		}
	} guard;
	Viewer = &listener;
	xl_named_audio_set_volume(32768);
	set_sound_sources();
	xl_named_audio_pause(0);
	xl_audio_sync_objects();
	const auto owner = [&](int index) {
		for (unsigned i = 0; i < owners.count; ++i)
			if (owners.entries[i].object == index) return owners.entries[i];
		throw std::runtime_error("Loaded ambient sound has no linked owner");
	};
	for (int index : { 49, 50, 51 }) {
		const auto entry = owner(index);
		const int volume = index == 49 ? 32768 : index == 50 ? 65536
		                                                     : 19661;
		check(entry.external && entry.sound == -1 && entry.sound_class == 1 && entry.decay == 1 && entry.ambient &&
		          entry.max_volume == volume && entry.max_distance == 256 * F1_0 && entry.loop_start == 0 && entry.loop_end == 0 &&
		          (entry.flags & sound_permanent) && (entry.channel >= 0) == (index != 50),
		      "Actual ambient initialization lost source volume, deferred ownership or disabled playback");
	}
	output();
	const auto serial = owner(50).serial;
	const auto operate = [&](int index) {
		xl_wall_trigger_reset();
		check(xl_wall_trigger(index, Players[Player_num].objnum, 0) == 0, "Ambient wall trigger was not consumed");
		xl_audio_sync_objects();
	};
	operate(2);
	check(xl_ambient_object_enabled(50) && owner(50).channel >= 0 && owner(50).serial == serial, "Enable did not start the existing disabled ambient owner");
	output();
	operate(7);
	check(Triggers[0].flags & TF_DISABLED, "Mixed effect/wall list did not reach its wall-trigger target");
	check(!xl_ambient_object_enabled(49) && owner(49).channel < 0 && owner(50).channel >= 0, "Disable removed the owner or stopped an unrelated sound");
	output();
	operate(9);
	check(!xl_ambient_object_enabled(50), "Second ambient disable failed");
	operate(8);
	check(xl_ambient_object_enabled(49) && !xl_ambient_object_enabled(50) && Triggers[8].type == 33 && (Triggers[8].flags & TF_DISABLED),
	      "Effect-target early stop lost order or post-action one-shot/alternate flags");
	output();
	const auto earned_objects = ambient_objects_snapshot();
	const auto earned_audio = named_audio_snapshot();
	check(ambient_audio_matches(earned_audio), "Earned ambient loops do not match authored resources");
	for (unsigned variant = 0; variant < 4; ++variant) {
		auto bad = earned_audio;
		for (auto &owner : bad["linked"]["owners"]) {
			if (owner["object"] != 50) continue; // Stopped owners must retain the correct resource too
			if (variant == 0) owner["sample"] = external_sample("sound.wav");
			if (variant == 1) owner["max_volume"] = 32768;
			if (variant == 2) owner["max_distance"] = 64 * F1_0;
			if (variant == 3) owner["loop_start"] = -1;
		}
		check(!ambient_audio_matches(bad), "Foreign ambient resource or loop parameters passed authored validation");
	}
	for (unsigned variant = 0; variant < 8; ++variant) {
		auto bad = earned_objects;
		if (variant == 0) bad.erase(0);
		if (variant == 1) bad[0][0] = 50;
		if (variant == 2) bad[0][1] = bad[0][1].get<int>() + 1;
		if (variant == 3) bad[0][1] = -1;
		if (variant == 4) bad[0][2] = 1;
		if (variant == 5) bad[0][3] = bool(0);
		if (variant == 6) bad[0][1] = UINT64_MAX;
		if (variant == 7) bad[0].push_back(0);
		check(!restore_ambient_objects(bad, false) && !restore_ambient_objects(bad, true) && ambient_objects_snapshot() == earned_objects && named_audio_snapshot() == earned_audio,
		      "Malformed ambient state changed object activation or audio");
	}
	constexpr const char *path = "xl-ambient-state.tmp";
	check(!PHYSFS_exists(path), "Ambient trailer control requires an unused temporary path");
	struct FileGuard {
		~FileGuard()
		{
			PHYSFS_delete("xl-ambient-state.tmp");
		}
	} file_guard;
	PHYSFS_file *file = PHYSFS_openWrite(path);
	REWIND_PHYSFS_FILE(writer, file);
	check(file && xl_wall_trigger_write(writer) && PHYSFS_close(file), "Cannot save ambient runtime trailer");
	check(xl_ambient_object_set_enabled(50, 1), "Cannot disturb earned ambient activation");
	xl_audio_sync_objects();
	file = PHYSFS_openRead(path);
	REWIND_PHYSFS_FILE(reader, file);
	check(file && xl_wall_trigger_read(reader) && PHYSFS_close(file) && ambient_objects_snapshot() == earned_objects && named_audio_snapshot() == earned_audio,
	      "Ambient trailer restore lost activation or earned playback");
	output();
	// Real FVI must ignore an invisible sound object in the player's segment
	fvi_query query{};
	query.p0 = &listener.pos;
	query.p1 = &Objects[49].pos;
	query.startseg = listener.segnum;
	query.thisobjnum = Players[Player_num].objnum;
	query.flags = FQ_CHECK_OBJS;
	fvi_info hit{};
	const int result = find_vector_intersection(&query, &hit);
	check(result != HIT_BAD_P0 && (result != HIT_OBJECT || (hit.hit_object != 49 && hit.hit_object != 50 && hit.hit_object != 51)), "Ambient sound object entered native collision or the query did not start inside its segment");
	Objects[49].type = OBJ_ROBOT;
	const int positive = find_vector_intersection(&query, &hit);
	Objects[49].type = 20;
	check(positive == HIT_OBJECT && hit.hit_object == 49, "Ambient collision query lacks a positive ordinary-object control");
	const auto native = Objects[49];
	++Objects[49].signature;
	xl_audio_sync_objects();
	check(!xl_ambient_object_set_enabled(49, 0), "Reused object slot inherited authored ambient activation");
	for (unsigned i = 0; i < owners.count; ++i) check(owners.entries[i].object != 49, "Reused object slot retained its old ambient loop");
	output();
	Objects[49] = native;
	check(restore_ambient_objects(earned_objects, true) && restore_named_audio(earned_audio, true), "Cannot recover earned ambient lifetimes after reuse control");
	output();
	obj_delete(51);
	xl_audio_sync_objects();
	check(!xl_ambient_object_set_enabled(51, 0) && !ambient_objects_snapshot()[2][3].get<bool>(), "Deleted ambient object retained live activation state");
	for (unsigned i = 0; i < owners.count; ++i) check(owners.entries[i].object != 51, "Deleted ambient source retained its loop");
	check(restore_ambient_objects(ambient_objects_snapshot(), true), "Deleted ambient lifetime cannot be restored");
	output();
}
} // namespace xl_ambient_objects_control
#endif
