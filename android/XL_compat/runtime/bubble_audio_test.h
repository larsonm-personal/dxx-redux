#ifndef DXX_XL_BUBBLE_AUDIO_TEST_H
#define DXX_XL_BUBBLE_AUDIO_TEST_H
#include "particle_objects.h"
#include "particle_state.h"
namespace xl_bubble_audio_control
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
			xl_particle_objects_clear();
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
		native.id = 0;
		native.signature = 4000 + index;
		native.control_type = CT_NONE;
		native.movement_type = MT_NONE;
		native.render_type = RT_NONE;
		native.segnum = listener.segnum;
		native.pos = listener.pos;
		native.pos.x += (index == 1 ? 10 : -10) * F1_0;
		native.pos.z += 10 * F1_0;
		native.orient = vmd_identity_matrix;
		auto &authored = source.objects[index];
		authored.type = 20;
		authored.id = authored.reader_id = 0;
		authored.control = authored.movement = 0;
		authored.render = authored.reader_render = 11;
		authored.segment = native.segnum;
		authored.position = { native.pos.x, native.pos.y, native.pos.z };
		authored.orientation = { F1_0, 0, 0, 0, F1_0, 0, 0, 0, F1_0 };
		// Original pmlev3 payload, with a disabled companion control
		authored.render_data = { 2, 2, 4, 2, 2, 9, 0, 0, 0, 100, 0, 2, index == 1 };
	}
	install_named_audio(prepare_named_audio(Gamesave_current_filename, source, ordinal));
	bind_particle_objects(source);
	const auto resource = load_audio_resource(bubble_sound_name, Gamesave_current_filename, ordinal);
	const auto pcm = decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource.bytes.data()), resource.bytes.size());
	const auto sample = prepare_audio_sample(pcm, 44100);
	const unsigned sample_id = unsigned(external_sample(bubble_sound_name));
	unsigned audible = 0;
	const auto output = [&] {
		const auto before = snapshot();
		begin_capture();
		xl_named_audio_pause(0);
		run_output(2048);
		for (std::size_t i = 0; i < captured_samples; ++i) {
			int expected = 0;
			for (const auto &voice : before.at("voices")) {
				require(voice[2] == sample_id, "Bubble output selected a different resource");
				const auto phase = (voice[3].get<std::size_t>() + i / 2) % sample.frames();
				const int panned = int(float(sample.stereo[phase * 2 + i % 2]) * (voice[11 + i % 2].get<float>() / 255.0f));
				expected = (std::clamp) (expected + panned * voice[10].get<int>() / 128, -32768, 32767);
			}
			require(captured[i] == expected, "Bubble actual spatial PCM differs");
			audible += expected != 0;
		}
	};
	int time = 0;
	const auto frame = [&] { particle_objects_frame(time += 25); };
	frame();
	require(owners.count == 1 && owners.active() == 1 && owners.entries[0].object == 1, "Bubble creation ignored initial activation");
	const auto &owner = owners.entries[0];
	require(owner.external && owner.sample == sample_id && owner.sound_class == 0 && owner.decay == 0 &&
	            owner.max_volume == F1_0 / 2 && owner.max_distance == 256 * F1_0 && owner.loop_start == -1 && owner.loop_end == -1,
	        "Bubble sound lost pinned source parameters");
	output();
	const auto saved_audio = snapshot(), saved_objects = particle_objects_snapshot();
	require(particle_audio_matches(saved_audio), "Valid bubble audio failed authored-state validation");
	for (unsigned variant = 0; variant < 7; ++variant) {
		auto bad = saved_audio;
		auto &row = bad["linked"]["owners"][0];
		if (variant == 0) row["class"] = 1;
		if (variant == 1) row["max_distance"] = F1_0;
		if (variant == 2) row["loop_start"] = 0;
		if (variant == 3) row["max_volume"] = F1_0;
		if (variant == 4) row["sample"] = external_sample("sound.wav");
		if (variant == 5) row["custom"] = true;
		if (variant == 6) row["external"] = false;
		require(!particle_audio_matches(bad) && snapshot() == saved_audio, "Malformed bubble audio changed state or passed validation");
	}
	require(restore_particle_objects(saved_objects, true) && restore_named_audio(saved_audio, true), "Bubble audio restore failed");
	time = 0;
	frame();
	require(snapshot() == saved_audio, "Rebuilt bubble duplicated or restarted restored audio");
	output();
	require(xl_particle_object_set_enabled(1, 0), "Cannot disable bubble control");
	// Prime the linked owner's cached slider before comparing a normal sync
	xl_audio_sync_objects();
	const auto before_disable = snapshot();
	frame();
	xl_audio_sync_objects();
	require(snapshot() == before_disable, "Release bubble disable stopped or restarted its generic sound");
	output();
	require(xl_particle_object_set_enabled(1, 1), "Cannot re-enable bubble control");
	const auto before_enable = snapshot();
	frame();
	require(snapshot() == before_enable, "Bubble re-enable duplicated or restarted its sound");
	Objects[1].pos.x = listener.pos.x + 1000 * F1_0;
	xl_audio_sync_objects();
	require(owners.count == 1 && !owners.active(), "Bubble range exit lost ownership or kept playback");
	const auto stopped = snapshot();
	require(restore_particle_objects(particle_objects_snapshot(), true) && restore_named_audio(stopped, true), "Stopped bubble restore failed");
	time = 0;
	frame();
	require(snapshot() == stopped, "Rebuilt bubble duplicated its stopped owner");
	Objects[1].pos.x = listener.pos.x + 10 * F1_0;
	xl_audio_sync_objects();
	require(owners.active() == 1, "Bubble range return did not resume");
	output();
	stop_time();
	const auto paused = snapshot();
	frame();
	start_time();
	require(snapshot() == paused, "Paused bubble changed audio ownership");
	++Objects[1].signature;
	frame();
	xl_audio_sync_objects();
	require(!owners.count, "Bubble recycled signature retained its old sound");
	require(xl_particle_object_set_enabled(2, 1), "Cannot activate companion bubble");
	frame();
	require(owners.count == 1 && owners.entries[0].object == 2 && owners.active() == 1, "Bubble activation omitted companion sound");
	output();
	Objects[2].type = OBJ_NONE;
	xl_audio_sync_objects();
	require(!owners.count && audible > 100, "Bubble deletion retained sound or PCM controls were silent");
	xl_particle_objects_clear();
	install_named_audio(original_bank);
	require(restore_named_audio(initial, true), "Bubble control failed to restore original audio state");
	std::cout << "Bubble audio passed: prepared addon, source parameters, actual spatial PCM, activation, release disable, range, pause, restored phase and signature lifetime\n";
}
} // namespace xl_bubble_audio_control
#endif
