// Actual native creation state, checked by extracted source in the existing runner
namespace xl_audio_creation_control
{
using xl_audio_creation_receipt::Fields;
void run(const std::filesystem::path &root, int source, const nlohmann::json &initial)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	using namespace named_audio_control;
	SDL_PauseAudio(1);
	ClassicAudioSpatial spatial;
	const auto point = [](const vms_vector &v) { return FixedLightPoint{ v.x, v.y, v.z }; };
	const auto saved_position = Objects[source].pos;
	const bool saved_backend = backend_ready;
	std::ostringstream receipt;
	receipt << "XL_AUDIO_CREATION 1\n";
	unsigned cases = 0;
	for (int object_link : { 0, 1, 2 })
		for (int sound_class = 0; sound_class < (object_link ? 7 : 1); ++sound_class)
			for (int forever : { 0, 1 })
				for (int defer : { 0, 1 })
					for (int volume : { -1, 0, 1, 32768, 65536, 131072 })
						for (int available : { 0, 1 })
							for (int distant : { 0, 1 }) {
								require(restore_named_audio(initial, true), "Cannot reset creation receipt state");
								// Stable fresh identities make source/native receipts directly comparable
								owners.next_serial = mixer.next_serial = 1;
								mixer.cursor = 0;
								backend_ready = available != 0;
								xl_audio_defer_objects(defer);
								auto position = saved_position;
								if (distant) position.x += 500 * F1_0;
								Objects[source].pos = position;
								const int resource = int(cases / 2 % 2), sample = object_link == 2 ? -1 : resource, range = (cases / 4 % 2 ? 64 : 256) * F1_0;
								const char *filename = resource ? "long.wav" : "sound.wav";
								const int decay = int(cases / 4 % 7), custom = int(cases / 4 % 2);
								const int start = cases / 4 % 3 ? -1 : 7, end = start < 0 ? -1 : 19;
								Fields input{ { "object", object_link }, { "sample", sample }, { "class", sound_class }, { "forever", forever }, { "deferred", defer }, { "volume", volume }, { "range", range }, { "loop_start", start }, { "loop_end", end }, { "decay", decay }, { "custom", custom }, { "available", available }, { "signature", int(Objects[source].signature) }, { "segment", int(Objects[source].segnum) }, { "side", 2 }, { "x", int(position.x) }, { "y", int(position.y) }, { "z", int(position.z) }, { "mapped", classic_sample(sample) } };
								input["resource"] = resource;
								if (object_link == 2) input["mapped"] = external_sample(filename);
								// Geometry is independently source-qualified. Supply every decay output,
								// so the source creation function still chooses which one it requests
								for (int mode = 0; mode <= 6; ++mode) {
									const auto value = spatial.output(Viewer->segnum, Objects[source].segnum, point(Viewer->pos), point(position), point(Viewer->orient.rvec), point(Viewer->orient.uvec), (std::max) (volume, 0), range, mode, GameCfg.ReverseStereo != 0);
									input["volume" + std::to_string(mode)] = value.volume;
									input["pan" + std::to_string(mode)] = value.pan;
								}
								const auto result = object_link == 2 ? xl_audio_link_external(filename, sound_class, source, forever, volume, range, start, end, decay, custom)
								                    : object_link    ? xl_audio_link_object(sample, sound_class, source, forever, volume, range, start, end, decay, custom)
								                                     : xl_audio_link_position(sample, Objects[source].segnum, 2, &position, forever, volume, range);
								Fields output{ { "result", result }, { "owners", owners.count }, { "voices", 0 } };
								if (owners.count) {
									require(owners.count == 1, "Fresh creation installed multiple owners");
									const auto &o = owners.entries[0];
									output["owner.external"] = o.external;
									output.insert({ { "owner.flags", o.flags }, { "owner.sound", o.sound }, { "owner.class", o.sound_class }, { "owner.channel", o.channel }, { "owner.volume", o.volume }, { "owner.pan", o.pan }, { "owner.max_volume", o.max_volume }, { "owner.range", o.max_distance }, { "owner.loop_start", o.loop_start }, { "owner.loop_end", o.loop_end }, { "owner.decay", o.decay }, { "owner.ambient", o.ambient }, { "owner.custom", o.custom }, { "owner.audio_volume", o.audio_volume }, { "owner.sample", o.sample } });
									if (object_link) {
										output["owner.signature"] = o.signature;
										output["owner.object"] = o.object - source;
									} else output.insert({ { "owner.segment", o.segment }, { "owner.side", o.side }, { "owner.x", o.position[0] }, { "owner.y", o.position[1] }, { "owner.z", o.position[2] } });
								}
								for (const auto &voice : mixer.voices) {
									if (!voice.remaining) continue;
									++output["voices"];
									output.insert({ { "voice.class", voice.priority.sound_class }, { "voice.persistent", voice.priority.persistent }, { "voice.kind", int(voice.kind) } });
									output.insert({ { "voice.sample", voice.sample }, { "voice.owner", voice.owner }, { "voice.remaining", voice.remaining }, { "voice.volume", voice.priority.volume }, { "voice.output_volume", voice.output.volume }, { "voice.left", voice.output.pan[0] }, { "voice.right", voice.output.pan[1] } });
								}
								require(owners.valid(mixer), "Creation receipt lost owner/channel bindings");
								receipt << "case " << cases++ << '\n';
								xl_audio_creation_receipt::write(receipt, input);
								xl_audio_creation_receipt::write(receipt, output);
							}
	backend_ready = saved_backend;
	Objects[source].pos = saved_position;
	require(restore_named_audio(initial, true), "Cannot restore audio after creation receipt");
	receipt << "end " << cases << '\n';
	const auto path = root / "audio-creation.txt";
	std::ifstream previous(path, std::ios::binary);
	const std::string old{ std::istreambuf_iterator<char>(previous), std::istreambuf_iterator<char>() };
	previous.close();
	if (old != receipt.str()) {
		std::ofstream output(path, std::ios::binary);
		output << receipt.str();
		require(bool(output), "Cannot write native audio creation receipt");
	}
	std::cout << "Native audio creation prepared: " << cases << " cases; actual source comparison required\n";
}
} // namespace xl_audio_creation_control
