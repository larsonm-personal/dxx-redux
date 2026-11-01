#ifndef DXX_XL_LIGHTNING_AUDIO_TEST_H
#define DXX_XL_LIGHTNING_AUDIO_TEST_H
#include "lightning_objects.h"
#include "../lightning_audio.h"
namespace xl_lightning_audio_control
{
inline void run(int ordinal)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	using namespace named_audio_control;
	SDL_PauseAudio(1);
	const auto original = xl_ambient_objects_control::source_level();
	const auto initial = snapshot();
	auto original_bank = prepare_named_audio(Gamesave_current_filename, original, ordinal);
	struct Guard {
		object first = Objects[1], second = Objects[2];
		object *viewer = Viewer;
		std::shared_ptr<const AudioResources> bank;
		~Guard()
		{
			SDL_PauseAudio(1);
			xl_lightning_objects_clear();
			Objects[1] = first;
			Objects[2] = second;
			Viewer = viewer;
			install_named_audio(std::move(bank));
		}
	} guard{ Objects[1], Objects[2], Viewer, original_bank };
	object listener = *Viewer;
	listener.orient = vmd_identity_matrix;
	Viewer = &listener;
	auto source = original;
	for (int index : { 1, 2 }) {
		auto &native = Objects[index];
		native.type = 20;
		native.id = 1;
		native.signature = 3000 + index;
		native.control_type = CT_NONE;
		native.movement_type = MT_NONE;
		native.render_type = RT_NONE;
		native.segnum = listener.segnum;
		native.pos = listener.pos;
		native.pos.x += (index == 1 ? 10 : -10) * F1_0;
		native.pos.z += 10 * F1_0;
		auto &authored = source.objects[index];
		authored.type = 20;
		authored.id = authored.reader_id = 1;
		authored.control = authored.movement = 0;
		authored.render = authored.reader_render = 12;
		authored.segment = native.segnum;
		authored.position = { native.pos.x, native.pos.y, native.pos.z };
		authored.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 0, 1, 1, 0, 0, 52, 149, 128, index == 1 };
	}
	install_named_audio(prepare_named_audio(Gamesave_current_filename, source, ordinal));
	bind_lightning_objects(source);
	const auto resource = load_audio_resource(lightning_sound_name, Gamesave_current_filename, ordinal);
	const auto pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource.bytes.data()), resource.bytes.size());
	const auto sample = prepare_audio_sample(pcm, 44100);
	const unsigned sample_id = unsigned(external_sample(lightning_sound_name));
	std::map<unsigned, AudioSample> expected_samples{ { sample_id, sample } };
	const auto output = [&] {
		const auto before = snapshot();
		begin_capture();
		xl_named_audio_pause(0);
		run_output(2048);
		for (std::size_t i = 0; i < captured_samples; ++i) {
			int expected = 0;
			for (const auto &voice : before.at("voices")) {
				const auto &expected_sample = expected_samples.at(voice[2].get<unsigned>());
				const auto phase = (voice[3].get<std::size_t>() + i / 2) % expected_sample.frames();
				const int panned = int(float(expected_sample.stereo[phase * 2 + i % 2]) * (voice[11 + i % 2].get<float>() / 255.0f));
				expected = (std::clamp) (expected + panned * voice[10].get<int>() / 128, -32768, 32767);
			}
			require(captured[i] == expected, "Lightning actual spatial PCM differs");
		}
	};
	int time = 2000000;
	const auto frame = [&] { lightning_objects_clock(time += 25); lightning_objects_frame(); };
	frame();
	require(owners.count == 2 && owners.active() == 2, "Lightning creation omitted enabled or initially disabled sound");
	for (unsigned i = 0; i < owners.count; ++i) {
		const auto &owner = owners.entries[i];
		require(owner.external && owner.sample == sample_id && owner.sound_class == 0 && owner.decay == 0 &&
		            owner.max_volume == F1_0 / 2 && owner.max_distance == 256 * F1_0 && owner.loop_start == -1 && owner.loop_end == -1,
		        "Lightning sound lost pinned creation parameters");
	}
	output();
	const auto saved_audio = snapshot(), saved_objects = lightning_objects_snapshot();
	require(lightning_audio_matches(saved_audio), "Valid lightning audio failed authored-state validation");
	for (unsigned variant = 0; variant < 7; ++variant) {
		auto bad = saved_audio;
		auto &owner = bad["linked"]["owners"][0];
		if (variant == 0) owner["class"] = 1;
		if (variant == 1) owner["max_distance"] = F1_0;
		if (variant == 2) owner["loop_start"] = 0;
		if (variant == 3) owner["max_volume"] = F1_0;
		if (variant == 4) owner["sample"] = external_sample("sound.wav");
		if (variant == 5) owner["custom"] = true;
		if (variant == 6) owner["external"] = false;
		require(!lightning_audio_matches(bad) && snapshot() == saved_audio, "Malformed lightning owner changed live state or passed validation");
	}
	require(restore_lightning_objects(saved_objects, true) && restore_named_audio(saved_audio, true), "Lightning audio restore failed");
	frame();
	require(snapshot() == saved_audio, "Rebuilt lightning duplicated or restarted restored audio");
	output();
	Objects[1].pos.x = listener.pos.x + 1000 * F1_0;
	xl_audio_sync_objects();
	require(owners.count == 2 && owners.active() == 1, "Lightning range lost ownership or failed to stop");
	output();
	Objects[1].pos.x = listener.pos.x + 10 * F1_0;
	xl_audio_sync_objects();
	require(owners.active() == 2, "Lightning range return failed to resume");
	output();
	stop_time();
	const auto paused = snapshot();
	frame();
	start_time();
	require(snapshot() == paused, "Paused lightning changed audio ownership");
	bool dormant = false;
	for (int i = 0; i < 50 && !dormant; ++i) {
		frame();
		const auto *emitter = lightning_object_emitter(1);
		dormant = std::none_of(emitter->bolts.begin(), emitter->bolts.end(), [](const auto &bolt) { return bolt.lifetime.nodes > 0; });
	}
	require(dormant && owners.count == 1 && owners.entries[0].object == 2, "Dormant lightning retained sound or disabled owner was updated");
	output();
	for (int i = 0; i < 50 && owners.count == 1; ++i) frame();
	require(owners.count == 2, "Lightning active nodes did not restart sound");
	output();
	xl_named_audio_stop();
	require(!owners.count, "Lightning stop retained sound owners");
	for (int i = 0; i < 50 && !owners.count; ++i) frame();
	require(owners.count == 1 && owners.entries[0].object == 1, "Muted lightning restart ignored activation");
	output();
	++Objects[1].signature;
	require(xl_audio_link_external(lightning_sound_name, 0, 1, 1, F1_0 / 2, 256 * F1_0, -1, -1, 0, 0) > 0,
	        "Cannot create recycled-slot companion sound");
	frame();
	require(owners.count == 1 && owners.entries[0].signature == Objects[1].signature,
	        "Lightning teardown retained the old lifetime or removed its recycled-slot companion");
	xl_lightning_objects_clear();
	xl_named_audio_stop();
	source.objects[1].render_data[1] = 0;
	source.objects[2].render_data[18] = 0;
	bind_lightning_objects(source);
	char forcefield_name[] = "ff_amb_1";
	const int forcefield = piggy_find_sound(forcefield_name);
	require(forcefield != 255, "Lightning forcefield control requires the original registered sound");
	const auto classic = prepare_classic_playback({});
	expected_samples.emplace(unsigned(classic_sample(forcefield)), prepare_audio_sample(classic.samples.at(forcefield).pcm, 44100));
	frame();
	require(owners.count == 2 && owners.active() == 2 && lightning_audio_matches(snapshot()), "Forcefield lightning omitted its companion or started a sound-disabled owner");
	output();
	Objects[1].pos.x = listener.pos.x + 1000 * F1_0;
	xl_audio_sync_objects();
	const auto forcefield_audio = snapshot();
	require(owners.count == 2 && !owners.active(), "Forcefield range control failed to stop both loops");
	require(restore_lightning_objects(lightning_objects_snapshot(), true) && restore_named_audio(forcefield_audio, true), "Stopped forcefield audio restore failed");
	frame();
	require(snapshot() == forcefield_audio, "Rebuilt forcefield duplicated its stopped companion");
	Objects[1].pos.x = listener.pos.x + 10 * F1_0;
	xl_audio_sync_objects();
	require(owners.count == 2 && owners.active() == 2, "Restored forcefield loops did not resume in range");
	output();
	xl_lightning_objects_clear();
	require(!owners.count, "Lightning clear retained the addon or forcefield companion");
	install_named_audio(original_bank);
	require(restore_named_audio(initial, true), "Lightning control failed to recover original bank state");
	// Guard restores object slots and the original bank after this isolated control
	std::cout << "Lightning audio passed: prepared addon, source parameters, actual spatial PCM, dormant/restart, disabled creation, range, pause, restore, object reuse and forcefield companion\n";
}
} // namespace xl_lightning_audio_control
#endif
