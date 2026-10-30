void afterburner_source_controls()
{
	using namespace ownership_reference;
	unsigned cases = 0, corrected = 0;
	for (int rate : { 11025, 22050, 44100 })
		for (int existing = 0; existing < 3; ++existing)
			for (unsigned flags = 0; flags < 64; ++flags) {
				reset(0);
				audio.m_objects.storage.fill({});
				localplayer.flags = flags & 1 ? PLAYER_FLAGS_AFTERBURNER : 0;
				gameStates.app.bPlayerIsDead = (flags & 2) != 0;
				gameStates.app.bEndLevelSequence = (flags & 4) != 0;
				gameData.physicsData.xAfterburnerCharge = flags & 8 ? 65536 : 0;
				controls[0].afterburnerState = (flags & 16) != 0;
				gameStates.gameplay.bLastAfterburnerState = flags & 32 ? 1 : 0;
				gameStates.gameplay.xLastAfterburnerCharge = flags & 32 ? 65536 : 0;
				options.sound.audioSampleRate = rate;
				spatial_volume = existing == 2 ? 0 : 65536;
				spatial_pan = 32767;
				if (existing) audio.CreateObjectSound(SOUND_AFTERBURNER_IGNITE, SOUNDCLASS_PLAYER, localplayer.nObject, 1, 65536, 256 * 65536, AFTERBURNER_LOOP_START, AFTERBURNER_LOOP_END);
				audio.start_calls = 0;
				DoAfterburnerStuff();
				const bool active = (flags & 1) && (flags & 8) && (flags & 16);
				const bool dead = flags & (2 | 4);
				const unsigned expected_owners = dead ? 0u : active      ? (existing == 2 ? 2u : 1u)
				                                         : existing == 2 ? 1u
				                                                         : 0u;
				if (audio.m_objects.ToS() != expected_owners || gameData.physicsData.xAfterburnerCharge != ((flags & 1) && (flags & 8) ? 65536 : 0) ||
				    gameStates.gameplay.bLastAfterburnerState != controls[0].afterburnerState || gameStates.gameplay.xLastAfterburnerCharge != gameData.physicsData.xAfterburnerCharge)
					throw std::runtime_error("Actual afterburner source lifecycle changed");
				const bool start = !dead && ((active && !existing) || (!active && existing == 1));
				if (audio.start_calls != unsigned(start)) throw std::runtime_error("Actual afterburner source emitted unexpected playback");
				if (start && (audio.request.sound != (active ? SOUND_AFTERBURNER_IGNITE : SOUND_AFTERBURNER_PLAY) || audio.request.sound_class != SOUNDCLASS_PLAYER || audio.request.volume != 65536 || audio.request.loop != int(active)))
					throw std::runtime_error("Actual afterburner source changed sound, class or loop policy");
				if (active && !dead) {
					const auto &owner = audio.m_objects[expected_owners - 1];
					if (owner.m_nLoopStart != (rate == 22050 ? 32027 : 16013) || owner.m_nLoopEnd != (rate == 22050 ? 48452 : 24226) || owner.m_maxVolume != 65536 || owner.m_maxDistance != 256 * 65536 || owner.m_soundClass != SOUNDCLASS_PLAYER)
						throw std::runtime_error("Actual afterburner source changed owner controls");
				}
				corrected += !dead && existing == 2;
				++cases;
			}
	reset(0);
	gameStates.app = {};
	gameStates.gameplay = {};
	localplayer.flags = 0;
	gameData.physicsData.xAfterburnerCharge = 0;
	controls[0].afterburnerState = 0;
	spatial_volume = 32768;
	std::cout << "PASS: " << cases << " actual source afterburner cases, including " << corrected << " stopped-owner accumulation/release cases requiring explicit repair\n";
}
