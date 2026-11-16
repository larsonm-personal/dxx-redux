#include "afterburner_animation.h"
#include "../xl_vclip_frame.h"
extern "C" {
#include "fireball.h"
}

static nlohmann::json afterburner_animation_control()
{
	const auto &clip = Vclip[VCLIP_AFTERBURNER_BLOB];
	require(clip.play_time > 0 && clip.frame_time > 0, "Afterburner control requires loaded clip timing");
	object emitter = *ConsoleObject;
	emitter.size = F1_0 / 16;
	compute_segment_center(&emitter.pos, &Segments[emitter.segnum]);
	emitter.orient = vmd_identity_matrix;
	// Fill through the real allocator with nonreclaimable lights, then reject both blobs
	std::vector<int> occupied;
	for (;;) {
		const int slot = obj_create(OBJ_LIGHT, 0, emitter.segnum, &emitter.pos, &vmd_identity_matrix,
		                            F1_0 / 16, CT_LIGHT, MT_NONE, RT_NONE);
		if (slot < 0) break;
		occupied.push_back(slot);
	}
	require(num_objects == object_capacity(), "Afterburner allocation control did not exhaust the native pool");
	const auto full = xl_afterburner_animation_snapshot();
	drop_afterburner_blobs(&emitter, 2, F1_0 / 16, 32769);
	require(num_objects == object_capacity() && xl_afterburner_animation_snapshot() == full,
	        "Failed afterburner allocation changed animation ownership");
	for (auto slot = occupied.rbegin(); slot != occupied.rend(); ++slot) obj_delete(*slot);
	unsigned created = 0;
	for (int count : { 1, 2 })
		for (int lifetime : { -1, 1, 16385, 32769, 98307 }) {
			std::set<int> signatures;
			for (int slot = 0; slot <= Highest_object_index; ++slot)
				if (Objects[slot].type != OBJ_NONE) signatures.insert(Objects[slot].signature);
			drop_afterburner_blobs(&emitter, count, F1_0 / 16, lifetime);
			unsigned found = 0;
			for (int slot = 0; slot <= Highest_object_index; ++slot) {
				const auto &object = Objects[slot];
				if (object.type == OBJ_NONE || signatures.count(object.signature)) continue;
				++found;
				require(object.type == OBJ_FIREBALL && object.id == VCLIP_AFTERBURNER_BLOB, "Afterburner created an unexpected object");
				const int duration = lifetime == -1 ? clip.play_time : lifetime;
				const int period = lifetime == -1 ? 0 : int(int64_t(clip.frame_time) * lifetime / clip.play_time);
				require(object.lifeleft == duration && object.rtype.vclip_info.frametime == period, "Native afterburner lost supplied lifetime or scaled period");
				for (int remaining : { duration, duration / 2, 0, -1 }) {
					int frame = -999;
					require(xl_afterburner_frame(slot, VCLIP_AFTERBURNER_BLOB, remaining, 30, &frame) &&
					            frame == dxx_xl_vclip_object_frame(clip.play_time, clip.frame_time, 30, remaining, lifetime == -1 ? 0 : lifetime, period),
					        "Native afterburner frame differs from source-tested selection");
				}
			}
			require(found == unsigned(count), "Native afterburner did not create every requested blob");
			created += found;
		}
	const auto before_reuse = xl_afterburner_animation_snapshot();
	const int reused_slot = before_reuse.at("objects").back()[0].get<int>();
	const int old_signature = Objects[reused_slot].signature;
	obj_delete(reused_slot);
	auto *replacement = object_create_explosion(emitter.segnum, &emitter.pos, F1_0 / 16, VCLIP_AFTERBURNER_BLOB);
	require(replacement && replacement - Objects == reused_slot && replacement->signature != old_signature,
	        "Afterburner reuse control did not replace its native slot identity");
	const auto after_reuse = xl_afterburner_animation_snapshot();
	for (const auto &row : after_reuse["objects"])
		if (row[0] == reused_slot) require(row[2] == 0 && row[3] == 0, "Afterburner slot reuse retained its old animation clock");
	require(xl_afterburner_lifetime(reused_slot, 32771), "Cannot assign replacement afterburner lifetime");
	const auto saved = xl_afterburner_animation_snapshot();
	require(xl_afterburner_animation_restore(saved, false), "Cannot validate earned afterburner clocks");
	for (int control = 0; control < 5; ++control) {
		auto invalid = saved;
		if (control == 0) invalid["objects"].erase(invalid["objects"].begin());
		if (control == 1) ++invalid["objects"][0][1].get_ref<nlohmann::json::number_integer_t &>();
		if (control == 2) invalid["objects"][0][2] = -1;
		if (control == 3) invalid["objects"][0][3] = 1;
		if (control == 4) invalid["objects"][0][2] = 0.5;
		require(!xl_afterburner_animation_restore(invalid, true) && xl_afterburner_animation_snapshot() == saved,
		        "Malformed afterburner clock changed its live owner");
	}
	std::cout << "Native afterburner lifetime controls: " << created << " blobs, allocation failure, slot reuse, five rejected state corruptions\n";
	return saved;
}

static nlohmann::json afterburner_animation_continuation()
{
	nlohmann::json observations = nlohmann::json::array();
	for (int tick = 0; tick <= 3; ++tick) {
		nlohmann::json frames = nlohmann::json::array();
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const auto &object = Objects[slot];
			if (object.type != OBJ_FIREBALL || object.id != VCLIP_AFTERBURNER_BLOB) continue;
			int frame = -999;
			require(xl_afterburner_frame(slot, object.id, object.lifeleft, Vclip[object.id].num_frames, &frame), "Afterburner continuation lost frame ownership");
			frames.push_back({ slot, int(object.signature), int(object.lifeleft), frame });
		}
		unsigned sim_state = 0;
		require(d_rand_get_stream_state(D_RNG_SIM, &sim_state), "Cannot inspect afterburner continuation SIM state");
		observations.push_back({ { "owner", xl_afterburner_animation_snapshot() }, { "frames", frames }, { "sim_state", sim_state }, { "sim_calls", d_rand_get_call_count() } });
		if (tick == 3) break;
		FrameTime = F1_0 / 60;
		Controls = {};
		calc_game_time();
		GameProcessFrame();
	}
	return observations;
}

static int afterburner_animation_resume(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		const char *extra = find_arg_value(argc, argv, "-extra-dir");
		require(mount_extra_dir(extra, error, sizeof(error)), error);
		require(load_base_mission("d2-xl", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		char save[] = "xl-pilot-state.sav";
		require(state_restore_all_sub(save, 0) != 0, "Fresh-process afterburner restore failed");
		std::ifstream file(std::filesystem::path(PHYSFS_getWriteDir()) / "afterburner-continuation.json");
		nlohmann::json expected;
		file >> expected;
		require(afterburner_animation_continuation() == expected, "Fresh-process afterburner continuation differs");
		std::cout << "Fresh-process afterburner clocks, remaining lifetimes, selected frames and SIM RNG matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Afterburner continuation failed: " << error.what() << '\n';
		return 1;
	}
}
