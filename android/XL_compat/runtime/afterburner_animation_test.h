#include "afterburner_animation.h"
#include "../xl_vclip_frame.h"
extern "C" {
#include "fireball.h"
}

static void afterburner_segment_controls(const object &source)
{
	unsigned rejected_points = 0;
	// Use original outer faces; move only a controlled emitter, never level geometry
	for (int segment = 0; segment <= Highest_segment_index && rejected_points != 3; ++segment)
		for (int side = 0; side < 6 && rejected_points != 3; ++side) {
			if (Segments[segment].children[side] >= 0) continue;
			vms_vector center;
			compute_center_point_on_side(&center, &Segments[segment], side);
			for (int axis = 0; axis < 3 && rejected_points != 3; ++axis)
				for (int sign : { -1, 1 }) {
					object emitter = source;
					emitter.segnum = segment;
					std::array<xl_compat::EmissionPoint, 3> basis{};
					basis[0][axis] = sign * F1_0;
					basis[1][(axis + 1) % 3] = sign * F1_0;
					basis[2][(axis + 2) % 3] = F1_0;
					emitter.orient.rvec = { basis[0][0], basis[0][1], basis[0][2] };
					emitter.orient.uvec = { basis[1][0], basis[1][1], basis[1][2] };
					emitter.orient.fvec = { basis[2][0], basis[2][1], basis[2][2] };
					emitter.pos = center;
					vm_vec_scale_add2(&emitter.pos, &emitter.orient.fvec, emitter.size);
					const auto points = xl_compat::afterburner_emission({ emitter.pos.x, emitter.pos.y, emitter.pos.z }, basis[2], basis[0],
					                                                    emitter.size, true, true, false, 0, 0);
					std::array<int, 2> segments;
					for (unsigned i = 0; i < 2; ++i) {
						vms_vector point{ points.positions[i][0], points.positions[i][1], points.positions[i][2] };
						segments[i] = find_point_seg(&point, segment);
					}
					if ((segments[0] < 0) == (segments[1] < 0)) continue;
					const unsigned rejected = segments[0] < 0 ? 0 : 1;
					if (rejected_points & (1u << rejected)) continue;
					std::set<int> signatures;
					for (int slot = 0; slot <= Highest_object_index; ++slot)
						if (Objects[slot].type != OBJ_NONE) signatures.insert(Objects[slot].signature);
					drop_afterburner_blobs(&emitter, 2, F1_0 / 16, 32769);
					unsigned found = 0;
					for (int slot = 0; slot <= Highest_object_index; ++slot) {
						const auto &created = Objects[slot];
						if (created.type == OBJ_NONE || signatures.count(created.signature)) continue;
						++found;
						require(created.type == OBJ_FIREBALL && created.id == VCLIP_AFTERBURNER_BLOB &&
						            xl_compat::EmissionPoint{ created.pos.x, created.pos.y, created.pos.z } == points.positions[1 - rejected] &&
						            created.segnum == segments[1 - rejected] && created.lifeleft == 32769,
						        "Rejected afterburner point changed its independently valid sibling");
						obj_delete(slot);
					}
					require(found == 1, "Per-point segment rejection did not preserve the valid afterburner point");
					rejected_points |= 1u << rejected;
				}
		}
	require(rejected_points == 3, "Original geometry did not exercise rejection of each afterburner point");
}

static nlohmann::json afterburner_animation_control()
{
	const auto &clip = Vclip[VCLIP_AFTERBURNER_BLOB];
	require(clip.play_time > 0 && clip.frame_time > 0, "Afterburner control requires loaded clip timing");
	object emitter = *ConsoleObject;
	emitter.size = F1_0 / 16;
	compute_segment_center(&emitter.pos, &Segments[emitter.segnum]);
	emitter.orient = vmd_identity_matrix;
	const auto emission_model = xl_prepared_afterburner_model(emitter.rtype.pobj_info.model_num, emitter.type == OBJ_PLAYER);
	require(emitter.type == OBJ_PLAYER && !emission_model.ase, "Afterburner lifetime fixture requires the original non-ASE player profile");
	const auto expected_emission = xl_compat::afterburner_emission({ emitter.pos.x, emitter.pos.y, emitter.pos.z }, { 0, 0, F1_0 }, { F1_0, 0, 0 },
	                                                               emitter.size, true, true, false, emission_model.low_z, emission_model.high_z);
	unsigned weapon_cases = 0;
	std::set<int> weapon_offsets;
	for (int id = 0; id < N_weapon_types; ++id) {
		const auto &weapon = Weapon_info[id];
		if (weapon.render_type != WEAPON_RENDER_POLYMODEL || weapon.afterburner_size <= 0) continue;
		object missile = emitter;
		missile.type = OBJ_WEAPON;
		missile.id = id;
		missile.render_type = RT_POLYOBJ;
		missile.rtype.pobj_info.model_num = weapon.model_num;
		const auto model = xl_prepared_afterburner_model(weapon.model_num, false);
		const auto point = xl_compat::afterburner_emission({ missile.pos.x, missile.pos.y, missile.pos.z }, { 0, 0, F1_0 }, { F1_0, 0, 0 },
		                                                   missile.size, false, true, model.ase, model.low_z, model.high_z);
		require(point.count == 1, "Weapon afterburner must emit one model-derived point");
		vms_vector position{ point.positions[0][0], point.positions[0][1], point.positions[0][2] };
		if (find_point_seg(&position, missile.segnum) < 0) continue;
		std::set<int> signatures;
		for (int slot = 0; slot <= Highest_object_index; ++slot)
			if (Objects[slot].type != OBJ_NONE) signatures.insert(Objects[slot].signature);
		drop_afterburner_blobs(&missile, 7, weapon.afterburner_size * F1_0 / 16, 32769);
		unsigned found = 0;
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const auto &created = Objects[slot];
			if (created.type == OBJ_NONE || signatures.count(created.signature)) continue;
			++found;
			require(created.type == OBJ_FIREBALL && created.id == VCLIP_AFTERBURNER_BLOB &&
			            xl_compat::EmissionPoint{ created.pos.x, created.pos.y, created.pos.z } == point.positions[0] && created.lifeleft == 32769,
			        "Native weapon afterburner differs from source-qualified model bounds");
			obj_delete(slot);
		}
		require(found == 1, "Native weapon afterburner did not preserve source count");
		weapon_offsets.insert(position.z - missile.pos.z);
		++weapon_cases;
	}
	require(weapon_cases >= 2 && weapon_offsets.size() >= 2, "Equal-size weapon control requires differing original model extents");
	afterburner_segment_controls(emitter);
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
	// The native reclaimer may replace the first fireball when admitting the second
	require(!occupied.empty(), "Afterburner allocation control has no releasable slot");
	const int available = occupied.back();
	occupied.pop_back();
	obj_delete(available);
	drop_afterburner_blobs(&emitter, 2, F1_0 / 16, 32769);
	const auto &single = Objects[available];
	require(num_objects == object_capacity() && single.type == OBJ_FIREBALL && single.id == VCLIP_AFTERBURNER_BLOB &&
	            xl_compat::EmissionPoint{ single.pos.x, single.pos.y, single.pos.z } == expected_emission.positions[1] && single.lifeleft == 32769,
	        "Afterburner allocation did not preserve native fireball reclamation");
	int single_frame = -999;
	require(xl_afterburner_frame(available, single.id, single.lifeleft, 30, &single_frame),
	        "Partial afterburner allocation lost animation ownership");
	obj_delete(available);
	for (auto slot = occupied.rbegin(); slot != occupied.rend(); ++slot) obj_delete(*slot);
	unsigned created = 0;
	for (int count : { 0, 1, 2, 7 })
		for (int lifetime : { -1, 1, 16385, 32769, 98307 }) {
			std::set<int> signatures;
			for (int slot = 0; slot <= Highest_object_index; ++slot)
				if (Objects[slot].type != OBJ_NONE) signatures.insert(Objects[slot].signature);
			drop_afterburner_blobs(&emitter, count, F1_0 / 16, lifetime);
			unsigned found = 0;
			std::set<xl_compat::EmissionPoint> emitted_positions;
			for (int slot = 0; slot <= Highest_object_index; ++slot) {
				const auto &object = Objects[slot];
				if (object.type == OBJ_NONE || signatures.count(object.signature)) continue;
				++found;
				emitted_positions.insert({ object.pos.x, object.pos.y, object.pos.z });
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
			require(found == expected_emission.count && emitted_positions == std::set<xl_compat::EmissionPoint>(expected_emission.positions.begin(), expected_emission.positions.end()),
			        "Native afterburner emission differs from source model-derived points/count");
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
	std::cout << "Native afterburner lifetime controls: " << created << " blobs, " << weapon_cases
	          << " weapon models, " << weapon_offsets.size() << " distinct equal-size offsets, both segment rejections, allocation failure/reclamation, slot reuse, five rejected state corruptions\n";
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
