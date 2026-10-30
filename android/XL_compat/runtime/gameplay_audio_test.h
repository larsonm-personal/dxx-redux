namespace xl_gameplay_audio_control
{
void run(int source, const nlohmann::json &initial)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	using namespace named_audio_control;
	struct MappingGuard {
		std::array<unsigned char, MAX_SOUNDS> sounds, alternate;
		int low_memory = GameArg.SysLowMem, slider = GameCfg.DigiVolume;
		MappingGuard()
		{
			std::copy_n(Sounds, MAX_SOUNDS, sounds.begin());
			std::copy_n(AltSounds, MAX_SOUNDS, alternate.begin());
		}
		~MappingGuard()
		{
			std::copy(sounds.begin(), sounds.end(), Sounds);
			std::copy(alternate.begin(), alternate.end(), AltSounds);
			GameArg.SysLowMem = low_memory;
			GameCfg.DigiVolume = slider;
		}
	} guard;
	Sounds[7] = 0;
	Sounds[8] = 1;
	Sounds[9] = 255;
	Sounds[SOUND_FORCEFIELD_HUM] = 0;
	AltSounds[7] = 8;
	AltSounds[8] = 9;
	GameArg.SysLowMem = 0;
	xl_looping_audio_control::run(initial);
	require(xl_gameplay_sound_sample(7) == 0 && xl_gameplay_sound_sample(9) == -1 && xl_gameplay_sound_sample(MAX_SOUNDS) == -1, "Native XL logical table translation differs");
	const auto reset = [&] { require(restore_named_audio(initial, true), "Cannot reset logical sound fixture"); };
	const auto output = [&] {
		const auto before = snapshot();
		begin_capture();
		xl_named_audio_pause(0);
		run_output(2048);
		xl_shared_audio_control::compare_output(before);
	};
	xl_seismic_audio_control::run(output);
	reset();
	int channel = xl_gameplay_sound_play(7, XL_SOUND_PLAYER, F1_0, 0, -1);
	require(channel >= 0 && mixer.voices[unsigned(channel)].sample == unsigned(classic_sample(0)) && mixer.voices[unsigned(channel)].priority.sound_class == XL_SOUND_PLAYER, "Logical player sound did not enter the shared pool");
	output();
	reset();
	GameArg.SysLowMem = 1;
	channel = xl_gameplay_sound_play(7, XL_SOUND_ROBOT, F1_0, 65536, -1);
	require(channel >= 0 && mixer.voices[unsigned(channel)].sample == unsigned(classic_sample(1)) && xl_gameplay_sound_sample(8) == -1, "Low-memory sound did not follow both tables");
	output();
	reset();
	channel = xl_gameplay_sound_play(-1, XL_SOUND_EXPLOSION, F1_0, 32767, -1);
	require(channel >= 0 && mixer.voices[unsigned(channel)].sample == unsigned(classic_sample(1)), "Negative physical PlaySound ID was translated as a logical sound");
	const auto earned = snapshot();
	require(xl_gameplay_sound_play(7, XL_SOUND_PLAYER, 0, 32767, -1) < 0 &&
	            xl_gameplay_sound_play(8, XL_SOUND_PLAYER, F1_0, 32767, -1) < 0 &&
	            xl_gameplay_sound_play(INT32_MIN, XL_SOUND_PLAYER, F1_0, 32767, -1) < 0 && snapshot() == earned,
	        "Silent, missing or invalid logical request changed playback");
	output();
	reset();
	GameArg.SysLowMem = 0;
	const auto identity = xl_gameplay_sound_object(7, XL_SOUND_ROBOT, source, 1, F1_0, 64 * F1_0, -1, -1, 0);
	require(identity > 0 && owners.count == 1 && owners.entries[0].sound == 0 && owners.entries[0].sound_class == XL_SOUND_ROBOT && owners.entries[0].decay == XL_SOUND_ROBOT, "Logical linked object lost its source class or physical sample");
	const auto linked = snapshot();
	require(xl_gameplay_sound_object(-1, XL_SOUND_ROBOT, source, 0, F1_0, 64 * F1_0, -1, -1, 0) < 0 && snapshot() == linked, "Linked creation incorrectly accepted a negative physical ID");
	output();
	unsigned forcefields = 0;
	for (int slider : { 0, 1, 4, 8 })
		for (int volume : { 0, 4096, 32768, 65536, 131072 }) {
			reset();
			GameCfg.DigiVolume = slider;
			xl_audio_defer_objects(1);
			const auto &object = Objects[source];
			require(xl_gameplay_sound_position(SOUND_FORCEFIELD_HUM, object.segnum, 2, &object.pos, 1, volume, 64 * F1_0) > 0 && owners.count == 1 &&
			            owners.entries[0].max_volume == audio_forcefield_volume(volume, slider * 8192),
			        "Native forcefield dispatch lost custom scaling or its minimum volume");
			xl_named_audio_stop();
			require(xl_gameplay_sound_position(7, object.segnum, 2, &object.pos, 1, volume, 64 * F1_0) > 0 && owners.entries[0].max_volume == volume,
			        "Forcefield scaling leaked into another logical sound with the same physical sample");
			++forcefields;
		}
	reset();
	std::cout << "Native logical audio passed: translated and negative physical playback, low-memory/sentinel maps, classed object PCM and " << forcefields << " forcefield slider/volume controls\n";
}
} // namespace xl_gameplay_audio_control
