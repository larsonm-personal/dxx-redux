// Native object/segment consumers and real PCM; source math is qualified separately
namespace xl_linked_audio_control
{
using namespace named_audio_control;
xl_compat::AudioOwner owner(std::int64_t serial)
{
	using namespace xl_compat::audio_runtime;
	AudioLock lock;
	for (unsigned i = 0; i < owners.count; ++i)
		if (owners.entries[i].serial == std::uint64_t(serial)) return owners.entries[i];
	throw std::runtime_error("Missing native linked sound identity");
}
void output()
{
	const auto before = snapshot();
	begin_capture();
	xl_named_audio_pause(0);
	run_output(2048);
	xl_shared_audio_control::compare_output(before);
}
void run(const nlohmann::json &initial)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	require(restore_named_audio(initial, true), "Cannot reset native linked audio");
	digi_set_digi_volume(32768);
	object listener = *Viewer;
	compute_segment_center(&listener.pos, &Segments[listener.segnum]);
	listener.orient = vmd_identity_matrix;
	const auto original_viewer = Viewer;
	Viewer = &listener;
	struct ViewerGuard {
		object *previous;
		~ViewerGuard()
		{
			Viewer = previous;
		}
	} viewer_guard{ original_viewer };
	auto position = listener.pos;
	position.x += 10 * F1_0;
	// Avoid the pinned ear-angle function's special zero angle at both unit-dot
	// boundaries; those axial cases already have dedicated source comparisons
	position.z += 10 * F1_0;
	const int source = obj_create(OBJ_MARKER, 0, listener.segnum, &position, &listener.orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(source >= 0, "Cannot create native sound source");
	struct ObjectGuard {
		int index;
		~ObjectGuard()
		{
			xl_named_audio_stop();
			obj_delete(index);
		}
	} object_guard{ source };
	const auto create = [&](int forever = 1, int custom = 0) {
		return xl_audio_link_object(0, 3, source, forever, F1_0, 64 * F1_0, -1, -1, 0, custom);
	};
	const auto reset = [&] {
		xl_named_audio_stop();
		xl_audio_defer_objects(0);
		Objects[source].pos = position;
		listener.orient = vmd_identity_matrix;
		digi_set_digi_volume(32768);
	};
	const auto token = create();
	require(token > 0 && owners.count == 1 && owner(token).channel >= 0 && owner(token).decay == 3 && owner(token).signature == Objects[source].signature, "Native object creation lost its source class, identity or channel");
	require(create() == token && owners.count == 1, "Live object duplicate allocated another owner");
	const auto first = owner(token);
	const auto first_voice = snapshot().at("voices")[0];
	output();
	Objects[source].pos.x -= 20 * F1_0;
	xl_audio_sync_objects();
	const auto moved = owner(token);
	require(moved.channel == first.channel && moved.serial == first.serial && moved.volume == first.volume && moved.pan != first.pan && snapshot().at("voices")[0][11] != first_voice[11],
	        ("Moving source did not update in place: channel " + std::to_string(first.channel) + "/" + std::to_string(moved.channel) + ", volume " + std::to_string(first.volume) + "/" + std::to_string(moved.volume) + ", pan " + std::to_string(first.pan) + "/" + std::to_string(moved.pan)).c_str());
	output();
	listener.orient.rvec.x = -F1_0;
	listener.orient.fvec.z = -F1_0;
	xl_audio_sync_objects();
	require(owner(token).pan == first.pan, "Rotating the listener did not reverse spatial pan");
	Objects[source].pos.x = listener.pos.x + 500 * F1_0;
	xl_audio_sync_objects();
	require(owners.count == 1 && owner(token).channel < 0 && snapshot().at("voices").empty(), "Inaudible forever owner was deleted or kept playing");
	Objects[source].pos = position;
	xl_audio_sync_objects();
	require(owner(token).channel >= 0 && owner(token).serial == std::uint64_t(token), "Audible owner did not restart with the same identity");
	output();
	digi_set_digi_volume(0);
	require(owners.count == 1 && owner(token).channel < 0, "Effects slider did not stop linked output");
	digi_set_digi_volume(32768);
	require(owner(token).channel >= 0, "Effects slider did not resume linked output");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[2], 2) == 1 && xl_audio_start_sample(1, 2, F1_0, 32767, 1, -1, -1) >= 0, "Cannot prepare unrelated named and ordinary voices");
	const auto unrelated = snapshot();
	++Objects[source].signature;
	xl_audio_sync_objects();
	require(!owners.count && snapshot().at("voices").size() == 2, "Reused native object retained its old sound or stopped unrelated voices");
	for (const auto &row : unrelated.at("voices"))
		if (row[7] != 1) {
			const auto after = snapshot().at("voices");
			require(std::find(after.begin(), after.end(), row) != after.end(), "Object reuse disturbed an unrelated voice");
		}
	output();
	reset();
	require(create(0) == -1 && !owners.count && snapshot().at("voices").size() == 1 && snapshot().at("voices")[0][7] == 2, "One-shot object sound became a persistent owner");
	reset();
	xl_audio_defer_objects(1);
	const auto pending = create();
	require(pending > 0 && (owner(pending).flags & sound_permanent) && owner(pending).channel < 0, "Deferred object started before activation");
	xl_audio_sync_objects();
	require(owner(pending).channel < 0, "Frame update ignored deferred startup");
	xl_audio_defer_objects(0);
	xl_audio_sync_objects();
	require(owner(pending).channel >= 0, "Deferred owner did not activate");
	reset();
	const auto custom = create(1, 1);
	const auto custom_voice = snapshot().at("voices")[0];
	Objects[source].pos.x -= 20 * F1_0;
	xl_audio_sync_objects();
	require(owner(custom).pan != first.pan && snapshot().at("voices")[0] == custom_voice, "Custom owner lost spatial updates or changed automatic channel controls");
	reset();
	const auto fixed = xl_audio_link_position(0, listener.segnum, 0, &position, 1, F1_0, 64 * F1_0);
	require(fixed > 0 && owner(fixed).channel >= 0 && owner(fixed).decay == 0 && owner(fixed).sound_class == 0, "Fixed segment sound did not start with generic linear decay");
	auto nearby = position;
	nearby.x += F1_0 / 2;
	require(xl_audio_link_position(0, listener.segnum, 0, &nearby, 1, F1_0, 64 * F1_0) == fixed, "Nearby segment duplicate was not reused");
	++nearby.x;
	require(xl_audio_link_position(0, listener.segnum, 0, &nearby, 1, F1_0, 64 * F1_0) > fixed && owners.count == 2, "Segment duplicate radius swallowed a different source");
	output();
	require(!xl_audio_destroy_position(listener.segnum, 1, 0) && owners.count == 2 && xl_audio_destroy_position(listener.segnum, 0, 0) && !owners.count, "Segment/side/sample destruction differs");
	reset();
	const auto retained = create();
	++Objects[source].signature;
	xl_audio_destroy_object(-1, -1);
	require(owners.count == 1 && owner(retained).channel < 0, "Missing-object cleanup did not retain the stopped forever owner");
	xl_audio_sync_objects();
	require(!owners.count, "Frame update did not delete the missing lifetime");
	require(restore_named_audio(initial, true), "Cannot leave linked controls at initial audio state");
	Mix_SetPostMix(nullptr, nullptr);
	std::cout << "Linked native audio passed: moving source/listener PCM, duplicate identities, range restart, slider sync, object reuse, mixed owners, one-shot, deferred/custom and segment destruction\n";
}
} // namespace xl_linked_audio_control
