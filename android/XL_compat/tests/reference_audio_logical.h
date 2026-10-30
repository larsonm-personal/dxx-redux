void audio_logical_source_controls()
{
	using namespace ownership_reference;
	static_assert(MAX_SOUNDS == xl_compat::audio_logical_sound_count);
	static_assert(XL_SOUND_GENERIC == SOUNDCLASS_GENERIC && XL_SOUND_AMBIENT == SOUNDCLASS_AMBIENT && XL_SOUND_PLAYER == SOUNDCLASS_PLAYER && XL_SOUND_ROBOT == SOUNDCLASS_ROBOT && XL_SOUND_LASER == SOUNDCLASS_LASER && XL_SOUND_MISSILE == SOUNDCLASS_MISSILE && XL_SOUND_EXPLOSION == SOUNDCLASS_EXPLOSION);
	unsigned translations = 0, forcefields = 0;
	reset(0);
	for (unsigned pattern = 0; pattern < 8; ++pattern) {
		for (unsigned profile = 0; profile < 2; ++profile)
			for (unsigned i = 0; i < MAX_SOUNDS; ++i) {
				Sounds[profile][i] = pattern && (i + pattern + profile) % 7 == 0 ? 255 : std::uint8_t((i * (profile ? 31 : 13) + pattern) % MAX_SOUNDS);
				AltSounds[profile][i] = pattern && (i + 3 * pattern + profile) % 11 == 0 ? 255 : std::uint8_t((i * 7 + 19 + pattern) % MAX_SOUNDS);
			}
		for (unsigned profile = 0; profile < 2; ++profile)
			for (int low_memory : { 0, 1 })
				for (int sound = -1; sound < MAX_SOUNDS; ++sound) {
					gameStates.sound.bD1Sound = int(profile);
					audio.m_info.bLoMem = low_memory != 0;
					if (xl_compat::audio_translate_sound(sound, Sounds[profile], AltSounds[profile], low_memory != 0) != audio.XlatSound(int16_t(sound)))
						throw std::runtime_error("Logical sound translation differs from actual source");
					++translations;
				}
	}
	for (int sound : { INT32_MIN, -32768, -254, 254, 255, 32767, INT32_MAX })
		if (xl_compat::audio_translate_sound(sound, Sounds[0], AltSounds[0], true) != -1) throw std::runtime_error("Invalid logical sound was not rejected");
	reset(0);
	Sounds[0][0] = 254;
	AltSounds[0][1] = 254;
	if (xl_compat::audio_translate_sound(0, Sounds[0], AltSounds[0], false) != -1 || xl_compat::audio_translate_sound(1, Sounds[0], AltSounds[0], true) != -1)
		throw std::runtime_error("Out-of-range authored sound mapping was not rejected");
	for (int slider = 0; slider <= 8; ++slider)
		for (int volume : { -1, 0, 1, 4095, 4096, 65535, 65536, 131072, INT32_MAX }) {
			reset(0);
			audio.m_objects.storage.fill({});
			gameStates.sound.bDontStartObjects = true;
			options.sound.xCustomSoundVolume = slider * 8192;
			CFixVector position;
			const int result = audio.CreateSegmentSound(SOUND_FORCEFIELD_HUM, 0, 0, position, 1, volume, 256 * 65536, nullptr);
			const int expected = result < 0 ? -1 : audio.m_objects[0].m_maxVolume;
			if (xl_compat::audio_forcefield_volume(volume, slider * 8192) != expected)
				throw std::runtime_error("Forcefield custom volume differs from source creation");
			++forcefields;
		}
	reset(0);
	options.sound.xCustomSoundVolume = 65536;
	std::cout << "PASS: " << translations << " actual source logical translations and " << forcefields << " forcefield creation scales, with explicit rejection of out-of-range mappings\n";
}
