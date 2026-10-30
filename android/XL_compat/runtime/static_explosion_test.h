#include "static_explosion_runtime.h"
#include "../tests/static_audio_receipt.h"
namespace xl_static_explosion_control
{
using namespace named_audio_control;
void record_initialization(const std::filesystem::path &root)
{
	using namespace xl_compat::audio_runtime;
	xl_static_audio_receipt::Case receipt;
	static_assert(MAX_SOUNDS == std::tuple_size<decltype(receipt.sounds)>::value);
	receipt.low_memory = GameArg.SysLowMem != 0;
	std::copy_n(Sounds, MAX_SOUNDS, receipt.sounds.begin());
	std::copy_n(AltSounds, MAX_SOUNDS, receipt.alternate.begin());
	for (int index = 0; const char *name = piggy_sound_name(index); ++index)
		if (!std::strcmp(name, "explode2")) {
			receipt.named_sample = index;
			break;
		}
	const auto source = xl_ambient_objects_control::source_level();
	for (unsigned i = 0; i < source.objects.size(); ++i) {
		const auto &object = source.objects[i];
		if (object.type == 19)
			receipt.objects.push_back({ int(i), object.id, object.render, object.render_data.at(0), Objects[i].render_type, Objects[i].rtype.vclip_info.vclip_num });
	}
	AudioLock lock;
	bool objects_started = false;
	for (unsigned i = 0; i < owners.count; ++i) {
		const auto &owner = owners.entries[i];
		if (owner.object < 0) {
			require(!objects_started, "Static initialization reordered surface and object owners");
			continue;
		}
		objects_started = true;
		require(owner.channel == -1, "Static initialization started audio before synchronization");
		receipt.owners.push_back({ owner.object, owner.sound, owner.sound_class, owner.max_volume, owner.max_distance,
		                           int(owner.flags), owner.loop_start, owner.loop_end, owner.decay, int(owner.external), int(owner.ambient) });
	}
	std::ofstream out(root / "static-audio.txt");
	xl_static_audio_receipt::write(out, receipt);
	require(bool(out), "Cannot record loaded static explosion initialization");
}
void output()
{
	const auto before = snapshot();
	begin_capture();
	xl_named_audio_pause(0);
	run_output(2048);
	// This fixture has no named WAV table; physical classic samples begin at zero
	xl_shared_audio_pcm::output(before, captured.data(), nullptr, captured_samples, 128, 0);
}
void run(const std::filesystem::path &root, int ordinal)
{
	char error[256]{};
	require(load_base_mission("xstatic", error, sizeof(error)), error);
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(ordinal < 0 ? 1 : ordinal);
	if (ordinal < 0) LoadLevel(ordinal, 0);
	SDL_PauseAudio(1);
	require(xl_level_extensions_active() && xl_source_level_version() == (ordinal == 3 ? 17 : ordinal == 2 ? 27
	                                                                                                       : 23),
	        "Static explosion fixture did not load");
	require(gamesave_level_data_warning_count() == 0, "Static explosion loader repaired its authored records");
	const int logical = xl_static_explosion_sound();
	require(logical >= 0, "Static explosion fixture requires the native named sound");
	Viewer = ConsoleObject;
	digi_set_digi_volume(32768);
	set_sound_sources();
	record_initialization(root);
	xl_audio_sync_objects();
	const auto initial = snapshot();
	const auto before_object = Objects[49];
	for (const auto &entry : std::array<std::pair<const char *, const char *>, 5>{ { { "stbadid.rl2", "missing vclip" }, { "stbadfr.rl2", "in-range initial frame" }, { "stbadct.rl2", "static_explosion" }, { "stbadmv.rl2", "unsupported movement layout" }, { "stbadrn.rl2", "static_explosion" } } }) {
		require(!xl_level_supported_at(entry.first, 0, 1) && std::strstr(xl_level_rejection(), entry.second),
		        (std::string("Static malformed admission lacks a specific rejection: ") + entry.first + ": " + xl_level_rejection()).c_str());
		require(snapshot() == initial && Objects[49].signature == before_object.signature && Objects[49].type == 19 &&
		            Objects[49].rtype.vclip_info.frametime == before_object.rtype.vclip_info.frametime && xl_level_extensions_active(),
		        "Rejected static level changed the current object or sound state");
	}
	// Despite its historical fixture name, a negative initial timer is legal XL
	require(xl_level_supported_at("stbadtm.rl2", 0, 1) && snapshot() == initial,
	        "Source-valid negative static animation timer was rejected or changed audio state");
	for (bool duration : { false, true }) {
		const auto clip = Vclip[0];
		if (duration) Vclip[0].frame_time = 0;
		else Vclip[0].play_time = 0;
		const bool rejected = !xl_level_supported_at(Gamesave_current_filename, 0, ordinal);
		const std::string reason = xl_level_rejection();
		Vclip[0] = clip;
		require(rejected && reason.find("positive timing") != std::string::npos && snapshot() == initial,
		        "Invalid static animation timing passed preflight or changed audio state");
	}
	require(xl_level_supported_at(Gamesave_current_filename, 0, ordinal) && snapshot() == initial, "Valid static admission did not recover after malformed controls");
	fvi_query query{};
	query.p0 = &ConsoleObject->pos;
	query.p1 = &Objects[49].pos;
	query.startseg = ConsoleObject->segnum;
	query.thisobjnum = Players[Player_num].objnum;
	query.flags = FQ_CHECK_OBJS;
	fvi_info hit{};
	const int collision = find_vector_intersection(&query, &hit);
	require(collision != HIT_BAD_P0 && (collision != HIT_OBJECT || (hit.hit_object != 49 && hit.hit_object != 50)), "Static explosion entered native collision");
	Objects[49].type = OBJ_ROBOT;
	const int positive = find_vector_intersection(&query, &hit);
	Objects[49].type = 19;
	require(positive == HIT_OBJECT && hit.hit_object == 49, "Static collision query lacks an ordinary-object positive control");
	dynamic_lighting(xl_ambient_objects_control::source_level(), Segments[0].verts[0]);
	for (int index : { 49, 50 }) {
		const auto &object = Objects[index];
		require(object.type == 19 && object.id == 0 && object.control_type == CT_POWERUP && object.movement_type == MT_NONE &&
		            object.render_type == RT_POWERUP && object.rtype.vclip_info.vclip_num == 0 && object.lifeleft == IMMORTAL_TIME,
		        "Static explosion load or source initializer changed identity, clip or lifetime");
		unsigned found = 0;
		for (const auto &owner : initial.at("linked").at("owners")) {
			if (owner.at("object") != index) continue;
			++found;
			require(owner.at("sound") == xl_gameplay_sound_sample(logical) && owner.at("class") == 1 && owner.at("max_volume") == F1_0 &&
			            owner.at("max_distance") == 256 * F1_0 && owner.at("loop_start") == -1 && owner.at("loop_end") == -1 &&
			            !owner.at("external").get<bool>() && owner.at("channel").get<int>() >= 0,
			        "Static explosion has incorrect permanent sound ownership");
		}
		require(found == 1, "Static explosion lacks exactly one sound owner");
	}
	output();
	std::ofstream receipt(root / "static-animation.txt");
	receipt << "XL_STATIC_ANIMATION 1\n";
	const auto old_time = FrameTime;
	unsigned count = 0;
	for (int index : { 49, 50 }) {
		auto &object = Objects[index];
		const auto &clip = Vclip[0];
		for (int initial_time : { -1, 0, clip.frame_time }) {
			object.rtype.vclip_info.frametime = initial_time;
			object.rtype.vclip_info.framenum = static_cast<sbyte>(clip.num_frames - 1);
			for (int elapsed : { 0, 1, clip.frame_time - 1, clip.frame_time, clip.frame_time + 1, clip.frame_time * clip.num_frames * 3 + 17, 1092, 2184 }) {
				const auto before = object.rtype.vclip_info;
				FrameTime = elapsed;
				do_powerup_frame(&object);
				receipt << index << ' ' << clip.num_frames << ' ' << clip.frame_time << ' ' << before.frametime << ' ' << int(before.framenum) << ' '
				        << elapsed << ' ' << object.rtype.vclip_info.frametime << ' ' << int(object.rtype.vclip_info.framenum) << '\n';
				++count;
			}
		}
	}
	FrameTime = old_time;
	receipt << "end " << count << '\n';
	require(bool(receipt), "Cannot write loaded static explosion animation receipt");
	receipt.close();
	xl_named_audio_pause(1);
	const auto earned = snapshot();
	const auto first = Objects[49].rtype.vclip_info, second = Objects[50].rtype.vclip_info;
	char save[] = "static-explosion.sav", description[] = "XL static explosions";
	require(!game_is_time_paused(), "Static explosion controls require running simulation");
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save static explosion state");
	require(!game_is_time_paused(), "Static explosion save unbalanced the simulation pause");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Static explosion stock transition failed");
	require(snapshot().at("source") == "" && snapshot().at("voices").empty(), "Static explosion sound leaked into Redux");
	require(!xl_static_explosion_animate(ConsoleObject) && !xl_static_explosion_draw(ConsoleObject), "XL static explosion dispatch intercepted Redux");
	require(state_restore_all_sub(save, 0) && Current_level_num == ordinal && snapshot() == earned, "Static explosion disk restore changed sound bindings or phase");
	require(Objects[49].rtype.vclip_info.frametime == first.frametime && Objects[49].rtype.vclip_info.framenum == first.framenum &&
	            Objects[50].rtype.vclip_info.frametime == second.frametime && Objects[50].rtype.vclip_info.framenum == second.framenum,
	        "Static explosion disk restore lost animation phase");
	output();
	++Objects[49].signature;
	xl_audio_sync_objects();
	const auto reused = snapshot();
	for (const auto &owner : reused.at("linked").at("owners"))
		require(owner.at("object") != 49, "Reused static explosion slot retained old sound ownership");
	Objects[50].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	xl_audio_sync_objects();
	require(Objects[50].type == OBJ_NONE, "Static explosion removal failed");
	const auto removed = snapshot();
	for (const auto &owner : removed.at("linked").at("owners"))
		require(owner.at("object") != 50, "Deleted static explosion retained a sound owner");
	std::cout << "Loaded static explosions " << (ordinal == 3 ? 17 : ordinal == 2 ? 27
	                                                                              : 23)
	          << "/" << xl_source_game_version() << ": initialization, odd/even animation, actual PCM, disk-through-stock and owner lifetime passed\n";
}
} // namespace xl_static_explosion_control
