// Execute pinned external creation and lookup before choosing port corrections
void external_audio_source_controls()
{
	using namespace ownership_reference;
	const auto require = [](bool value, const char *message) {
		if (!value) throw std::runtime_error(message);
	};
	const auto fresh = [&] {
		reset(0);
		audio.m_objects.storage.fill({});
		audio.m_info.nNextSignature = 1;
		spatial_volume = 32768;
		spatial_pan = 16384;
	};
	unsigned cases = 0;
	for (int sound_class = 0; sound_class <= 6; ++sound_class)
		for (bool deferred : { false, true })
			for (bool custom : { false, true })
				for (bool available : { false, true }) {
					fresh();
					gameStates.sound.bDontStartObjects = deferred;
					audio.fail_start = !available;
					const auto result = audio.CreateObjectSound(-1, sound_class, 0, 1, 65536, 256 * 65536, 3, 9, "ambient.wav", 6, custom);
					const auto &owner = audio.m_objects[0];
					require(result == 1 && audio.m_objects.ToS() == 1 && owner.m_nSound == -1 && std::string(owner.m_szSound) == "ambient.wav", "Source external creation lost its resource identity");
					require(owner.m_nDecay == sound_class && owner.m_soundClass == sound_class && owner.m_bAmbient == (sound_class == SOUNDCLASS_AMBIENT) && owner.m_bCustom == custom, "Source external class, decay, ambient or custom policy changed");
					require(owner.m_linkType.obj.nObject == 0 && owner.m_linkType.obj.nObjSig == objects[0].signature && owner.m_nLoopStart == 3 && owner.m_nLoopEnd == 9, "Source external owner lost lifetime or loop markers");
					require((owner.m_channel >= 0) == (!deferred && available) && bool(owner.m_flags & SOF_PERMANENT) == deferred, "Source external startup/deferred behavior changed");
					if (!deferred) require(audio.request.sound == -1 && audio.request.filename == "ambient.wav" && audio.request.loop == 1 && audio.request.start == 3 && audio.request.end == 9 && audio.request.owner == 0, "Source external request was translated or lost its resource");
					++cases;
				}
	fresh();
	const auto first = audio.CreateObjectSound(-1, SOUNDCLASS_AMBIENT, 0, 1, 65536, 256 * 65536, 0, 0, "ambient.wav");
	const auto repeat = audio.CreateObjectSound(-1, SOUNDCLASS_AMBIENT, 0, 1, 65536, 256 * 65536, 0, 0, "ambient.wav");
	require(first != repeat && audio.m_objects.ToS() == 2 && audio.start_calls == 2, "Pinned external duplicate-lookup defect no longer reproduces");
	fresh();
	const auto ordinary = audio.CreateObjectSound(0, SOUNDCLASS_GENERIC, 0, 1);
	const auto external = audio.CreateObjectSound(-1, SOUNDCLASS_AMBIENT, 0, 1, 65536, 256 * 65536, 0, 0, "ambient.wav");
	require(external == ordinary && audio.m_objects.ToS() == 1 && audio.start_calls == 1 && audio.m_objects[0].m_nSound == 0 && !*audio.m_objects[0].m_szSound, "Pinned external/physical-zero lookup alias no longer reproduces at coincident position");
	fresh();
	std::cout << "PASS: " << cases << " actual source external creations; reproduced duplicate external owners and coincident physical-zero alias\n";
}
