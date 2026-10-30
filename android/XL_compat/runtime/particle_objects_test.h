#include "particle_objects.h"
#include "object_trigger.h"
#include "trigger_state.h"

static void particle_objects_native_case(bool active)
{
	using namespace xl_compat;
	const auto sim_before = d_rand_get_stream_call_count(D_RNG_SIM), fx_before = d_rand_get_stream_call_count(D_RNG_FX);
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
	xl_particle_objects_clear();
	require(particle_systems().empty() && !xl_particle_object_set_enabled(1, 1), "Particle clear left a system or activation binding");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == sim_before, "Particle lifecycle or activation consumed simulation RNG");
	std::cout << "XL particle native ownership: wall/object activation, cadence, drain/re-enable, recycled signatures, pause, point jitter, visible scene/render cadence, atomic binding and SIM isolation passed\n";
}
