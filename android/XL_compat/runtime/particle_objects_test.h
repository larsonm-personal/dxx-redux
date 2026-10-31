#include "particle_objects.h"
#include "particle_state.h"
#include "transparency_scene.h"
#include "object_trigger.h"
#include "trigger_state.h"
#include <nlohmann/json.hpp>

static void particle_saved_state_case()
{
	using namespace xl_compat;
	const auto sim = d_rand_get_stream_call_count(D_RNG_SIM);
	particle_objects_frame(25);
	require(particle_systems().size() == 1 && particle_systems().front().emitter.count > 0, "Particle save control requires a live cloud");
	const auto earned = particle_objects_snapshot();
	require(earned.size() == 2 && earned[0][2].get<bool>() && !earned[1][2].get<bool>(), "Particle save control requires mixed activation");
	constexpr const char *path = "xl-particle-state.tmp";
	require(!PHYSFS_exists(path), "Particle save control requires an unused temporary path");
	struct FileGuard {
		~FileGuard()
		{
			PHYSFS_delete("xl-particle-state.tmp");
		}
	} file_guard;
	const auto capture = [&] {
		auto *file = PHYSFS_openWrite(path);
		REWIND_PHYSFS_FILE(writer, file);
		require(file && xl_wall_trigger_write(writer) && PHYSFS_close(file), "Cannot capture particle runtime trailer");
		file = PHYSFS_openRead(path);
		require(file && PHYSFS_fileLength(file) >= 16, "Cannot read particle runtime trailer");
		Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
		const auto consumed = PHYSFS_readBytes(file, bytes.data(), bytes.size());
		require(PHYSFS_close(file) && consumed == PHYSFS_sint64(bytes.size()), "Particle runtime trailer truncated");
		return bytes;
	};
	const auto encoded = capture();
	const auto original = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
	const auto restore = [&](const Bytes &bytes) {
		auto *file = PHYSFS_openWrite(path);
		require(file, "Cannot open particle trailer control");
		const auto written = PHYSFS_writeBytes(file, bytes.data(), bytes.size());
		require(PHYSFS_close(file) && written == PHYSFS_sint64(bytes.size()), "Cannot write particle trailer control");
		file = PHYSFS_openRead(path);
		REWIND_PHYSFS_FILE(reader, file);
		require(file, "Cannot open particle trailer for restoration");
		const int accepted = xl_wall_trigger_read(reader);
		require(PHYSFS_close(file), "Cannot close particle trailer control");
		return accepted;
	};
	const auto *cloud = &particle_systems().front();
	const int count = cloud->emitter.count;
	const auto fx = d_rand_get_stream_call_count(D_RNG_FX);
	for (int variant = 0; variant < 12; ++variant) {
		auto changed = original;
		auto &rows = changed["particle_objects"];
		if (variant == 0) rows[0][0] = 2;
		if (variant == 1) rows[0][1] = -1;
		if (variant == 2) rows[0][1] = UINT64_MAX;
		if (variant == 3) rows[0][1] = rows[0][1].get<int>() + 1;
		if (variant == 4) rows[0][2] = 1;
		if (variant == 5) rows[0][3] = bool(0);
		if (variant == 6) rows[0].push_back(0);
		if (variant == 7) rows.erase(rows.begin());
		if (variant == 8) rows.push_back(rows[0]);
		if (variant == 9) changed.erase("particle_objects");
		if (variant == 10) changed["source"] = original["source"].get<std::uint64_t>() ^ 1u;
		if (variant == 11) rows = nlohmann::json::object();
		const auto payload = changed.dump();
		Bytes bytes(encoded.begin(), encoded.begin() + 16);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		for (unsigned i = 0; i < 4; ++i) {
			bytes[8 + i] = static_cast<unsigned char>(payload.size() >> (i * 8));
			bytes[12 + i] = static_cast<unsigned char>(checksum >> (i * 8));
		}
		require(!restore(bytes), "Malformed particle state passed full trailer restoration");
		require(capture() == encoded && &particle_systems().front() == cloud && cloud->emitter.count == count, "Rejected particle state changed owned runtime state or live cloud");
	}
	require(d_rand_get_stream_call_count(D_RNG_FX) == fx, "Particle save validation consumed effects RNG");
	require(xl_particle_object_set_enabled(1, 0) && xl_particle_object_set_enabled(2, 1), "Cannot disturb saved particle activation");
	require(restore(encoded) && particle_objects_snapshot() == earned && particle_systems().empty(), "Particle restore lost authored activation or retained transient clouds");
	require(d_rand_get_stream_call_count(D_RNG_FX) == fx && d_rand_get_stream_call_count(D_RNG_SIM) == sim, "Particle restore consumed RNG");
	particle_objects_frame(0);
	require(particle_systems().size() == 1 && particle_systems().front().object == 1 && particle_systems().front().emitter.count == 0, "Restored activation did not create one fresh owner");
	particle_objects_frame(25);
	require(particle_systems().front().emitter.count > 0, "Restored particle owner did not resume emission");
	const auto saved_object = Objects[2];
	Objects[2].type = OBJ_NONE;
	const auto removed = particle_objects_snapshot();
	require(!removed[1][3].get<bool>() && restore_particle_objects(removed, true) && !xl_particle_object_set_enabled(2, 1), "Deleted particle lifetime resurrected on restore");
	Objects[2] = saved_object;
	require(!restore_particle_objects(removed, false), "Dead particle state accepted a recycled native owner");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == sim, "Particle regeneration changed simulation RNG");
	std::cout << "XL particle saved state: full-trailer activation round trip, 12 atomic rejection controls, fresh clouds, deleted owners and RNG isolation passed\n";
}

static void particle_objects_native_case(bool active)
{
	using namespace xl_compat;
	const auto sim_before = d_rand_get_stream_call_count(D_RNG_SIM), fx_before = d_rand_get_stream_call_count(D_RNG_FX);
	xl_transparency_test_controls(active);
	if (!active) {
		xl_particle_objects_frame();
		particle_objects_frame(1000);
		xl_particle_render_scene(nullptr, 0);
		require(particle_scene({}, {}, {}, 1000, 128, 128).empty(), "Stock level produced an XL particle scene");
		require(!xl_particle_object_set_enabled(1, 1) && particle_systems().empty(), "Stock level accepted an XL particle owner");
		require(d_rand_get_stream_call_count(D_RNG_SIM) == sim_before && d_rand_get_stream_call_count(D_RNG_FX) == fx_before, "Stock particle hooks consumed RNG");
		return;
	}
	require(!game_is_time_paused() && Highest_object_index >= 3 && Num_triggers > 0 && Players[Player_num].objnum == 0, "Particle owner controls require live unpaused Neptune");
	PHYSFS_file *file = PHYSFS_openRead(Gamesave_current_filename);
	require(file && PHYSFS_fileLength(file) > 0, "Particle owner source missing");
	Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
	const auto consumed = PHYSFS_readBytes(file, bytes.data(), bytes.size());
	PHYSFS_close(file);
	require(consumed == PHYSFS_sint64(bytes.size()), "Particle owner source truncated");
	const auto original = inspect_level(bytes);
	auto source = original;
	struct Guard {
		object first = Objects[1], second = Objects[2], actor = Objects[3];
		trigger action = Triggers[0];
		~Guard()
		{
			Objects[1] = first;
			Objects[2] = second;
			Objects[3] = actor;
			Triggers[0] = action;
			xl_particle_objects_clear();
		}
	} guard;
	for (int index : { 1, 2 }) {
		auto &native = Objects[index];
		native.type = 20;
		native.signature = 100 + index;
		native.id = 0;
		native.control_type = CT_NONE;
		native.movement_type = MT_NONE;
		native.render_type = RT_NONE;
		native.segnum = 0;
		compute_segment_center(&native.pos, &Segments[0]);
		auto &authored = source.objects[index];
		authored.type = 20;
		authored.id = authored.reader_id = 0;
		authored.control = authored.movement = 0;
		authored.render = authored.reader_render = 11;
		authored.segment = native.segnum;
		authored.position = { native.pos.x, native.pos.y, native.pos.z };
		authored.orientation = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		authored.render_data = { 1, 1, 1, 1, 1, 10, 255, 128, 0, 128, 1, 3, 0 };
	}
	bind_particle_objects(source);
	particle_objects_frame(0);
	require(particle_systems().empty() && d_rand_get_stream_call_count(D_RNG_FX) == fx_before, "Disabled authored particles allocated systems or consumed RNG");
	const auto wall = [&](int type, int count) {
		Triggers[0] = {};
		Triggers[0].type = static_cast<ubyte>(type);
		Triggers[0].time = -1;
		Triggers[0].num_links = static_cast<short>(count);
		for (int i = 0; i < count; ++i) {
			Triggers[0].seg[i] = static_cast<short>(1 + i);
			Triggers[0].side[i] = -1;
		}
		xl_wall_trigger_reset();
		require(xl_wall_trigger(0, Players[Player_num].objnum, 0) == 0, "Particle activation was not consumed by the wall trigger");
	};
	wall(32, 2);
	particle_objects_frame(0);
	require(particle_systems().size() == 2 && particle_systems().front().object == 2, "Authored particle systems lost newest-first source order");
	particle_objects_frame(24);
	require(particle_systems().front().emitter.moved == 0 && d_rand_get_stream_call_count(D_RNG_FX) == fx_before, "Particle manager bypassed the 25-ms update gate");
	particle_objects_frame(25);
	require(particle_systems().front().emitter.count > 0 && d_rand_get_stream_call_count(D_RNG_FX) > fx_before, "Enabled native emitter produced no particles");
	const auto render_calls = d_rand_get_stream_call_count(D_RNG_FX);
	const auto &first_emitter = particle_systems().front().emitter;
	const auto &first_particle = first_emitter.slots[std::size_t(first_emitter.first)];
	const int rotation_before = first_particle.rotation;
	const ParticlePoint eye{ Objects[1].pos.x, Objects[1].pos.y, Objects[1].pos.z };
	const ParticleBasis camera{ { 65536, 0, 0 }, { 0, 65536, 0 } };
	const auto first_scene = particle_scene({ 0 }, eye, camera, 32, 128, 128);
	require(!first_scene.empty() && first_particle.rotation == rotation_before, "Native particle scene bypassed render cadence or omitted visible particles");
	const auto next_scene = particle_scene({ 0 }, eye, camera, 33, 128, 128);
	require(next_scene.size() == first_scene.size() && first_particle.rotation == (rotation_before + 1) % 64, "Native particle scene did not advance its source rotation phase");
	require(particle_scene({}, eye, camera, 34, 128, 128).empty(), "Invisible particle scene produced quads");
	// Visibility alone does not render a particle rejected by the depth bound
	auto &clipped = const_cast<FireParticle &>(first_particle);
	const auto unclipped_position = clipped.position;
	const int unclipped_rotation = clipped.rotation;
	clipped.position[0] += 1000 * 65536;
	particle_scene({ 0 }, eye, camera, 66, 128, 128);
	require(clipped.rotation == unclipped_rotation, "Depth-rejected particle advanced render animation");
	clipped.position = unclipped_position;
	require(d_rand_get_stream_call_count(D_RNG_FX) == render_calls && d_rand_get_stream_call_count(D_RNG_SIM) == sim_before, "Particle scene construction consumed RNG");
	const auto *old = &particle_systems().back();
	wall(33, 1);
	particle_objects_frame(26);
	require(old->object == 1 && old->emitter.life == 0 && old->emitter.count > 0, "Disabling removed live particles instead of draining them");
	wall(32, 1);
	particle_objects_frame(27);
	require(particle_systems().size() == 3 && &particle_systems().front() != old && particle_systems().front().object == 1 && particle_systems().front().emitter.life < 0, "Re-enable did not allocate a fresh emitter beside the draining one");
	const auto *replacement = &particle_systems().front();
	const auto pause_calls = d_rand_get_stream_call_count(D_RNG_FX);
	stop_time();
	particle_objects_frame(3000);
	xl_particle_objects_frame();
	start_time();
	require(!game_is_time_paused() && particle_systems().front().emitter.moved == 27 && d_rand_get_stream_call_count(D_RNG_FX) == pause_calls, "Paused particles advanced or consumed RNG");
	for (int time = 3000; time <= 6000; time += 25) particle_objects_frame(time);
	require(particle_systems().size() == 2 && &particle_systems().front() == replacement, "Draining old emitter unbound or recreated its replacement");
	++Objects[2].signature;
	particle_objects_frame(6025);
	require(!xl_particle_object_set_enabled(2, 1), "Particle owner followed a recycled object signature");
	--Objects[2].signature;
	require(!xl_particle_object_set_enabled(2, 1), "Dead particle binding resurrected after restoring an old signature");
	for (int time = 6050; time <= 9000; time += 25) particle_objects_frame(time);
	require(particle_systems().size() == 1 && particle_systems().front().object == 1, "Recycled particle owner did not drain");
	// A real automatic object-trigger path must reach the same authored switch
	auto actions = source;
	actions.object_trigger_records.clear();
	ObjectTrigger disable{};
	Objects[3].type = OBJ_POWERUP;
	disable.object = 3;
	disable.action.type = 33;
	disable.action.flags = 128;
	disable.action.time = -1;
	disable.action.targets = { { 1, -1 } };
	actions.object_trigger_records.push_back(disable);
	xl_object_trigger_load(actions);
	xl_object_trigger_frame();
	xl_object_trigger_state action_state{};
	require(xl_object_trigger_state_get(0, &action_state) && action_state.operated, "Particle automatic object trigger did not operate");
	for (int time = 9025; time <= 12000; time += 25) particle_objects_frame(time);
	require(particle_systems().empty(), "Object-trigger disable did not drain the final particle system");
	xl_object_trigger_load(original);
	// Point emitters consume two six-draw jitter visits per game frame, even
	// before the manager's next 25-ms update; face emitters consume none
	source.objects[1].render_data[10] = 0;
	source.objects[1].render_data[12] = 1;
	bind_particle_objects(source);
	const auto jitter_calls = d_rand_get_stream_call_count(D_RNG_FX);
	particle_objects_frame(0);
	require(particle_systems().size() == 1 && d_rand_get_stream_call_count(D_RNG_FX) == jitter_calls + 12, "Native point emitter lost one of the source's two jitter visits");
	const auto *preserved = &particle_systems().front();
	auto malformed = source;
	malformed.objects[1].render_data[11] = 0;
	bool rejected = false;
	try {
		bind_particle_objects(malformed);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && &particle_systems().front() == preserved, "Failed particle binding partially replaced live ownership");
	Triggers[0] = guard.action;
	particle_saved_state_case();
	xl_particle_objects_clear();
	require(particle_systems().empty() && !xl_particle_object_set_enabled(1, 1), "Particle clear left a system or activation binding");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == sim_before, "Particle lifecycle or activation consumed simulation RNG");
	std::cout << "XL particle native ownership: wall/object activation, cadence, drain/re-enable, recycled signatures, pause, point jitter, visible scene/render cadence, atomic binding and SIM isolation passed\n";
}
