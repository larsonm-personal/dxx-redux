// Compare real native receipts to freshly initialized actual source creation
xl_compat::AudioPriority audio_channel_reference_priority(int, int, int, int, int);
void audio_creation_runtime_controls(const char *path)
{
	using namespace ownership_reference;
	using xl_audio_creation_receipt::Fields;
	std::ifstream stream(path);
	std::string tag;
	int version = 0;
	if (!(stream >> tag >> version) || tag != "XL_AUDIO_CREATION" || version != 1) throw std::runtime_error("Invalid creation receipt header");
	unsigned cases = 0, ordinal = 0;
	while (stream >> tag >> ordinal) {
		if (tag == "end") break;
		if (tag != "case" || ordinal != cases) throw std::runtime_error("Creation receipt cases are missing or reordered");
		const auto input = xl_audio_creation_receipt::read(stream);
		const auto native = xl_audio_creation_receipt::read(stream);
		const auto value = [&](const char *key) { return int(input.at(key)); };
		reset(0);
		audio.m_objects.storage.fill({});
		audio.m_info.nNextSignature = 1;
		audio.fail_start = value("available") == 0;
		gameStates.sound.bDontStartObjects = value("deferred") != 0;
		auto &object = objects[0];
		object.signature = object.info.nSignature = value("signature");
		object.info.nSegment = value("segment");
		object.info.position.vPos = { value("x"), value("y"), value("z") };
		creation_input = &input;
		const auto result = value("object") ? audio.CreateObjectSound(int16_t(value("sample")), value("class"), 0, value("forever"), value("volume"), value("range"), value("loop_start"), value("loop_end"), nullptr, value("decay"), uint8_t(value("custom")), 0) : audio.CreateSegmentSound(int16_t(value("sample")), int16_t(value("segment")), int16_t(value("side")), object.info.position.vPos, value("forever"), value("volume"), value("range"), nullptr);
		creation_input = nullptr;
		Fields expected{ { "result", result }, { "owners", audio.m_objects.ToS() }, { "voices", 0 } };
		if (audio.m_objects.ToS()) {
			if (audio.m_objects.ToS() != 1) throw std::runtime_error("Fresh source creation installed multiple owners");
			const auto &o = audio.m_objects[0];
			expected.insert({ { "owner.flags", o.m_flags }, { "owner.sound", o.m_nSound }, { "owner.class", o.m_soundClass }, { "owner.channel", o.m_channel }, { "owner.volume", o.m_volume }, { "owner.pan", o.m_pan }, { "owner.max_volume", o.m_maxVolume }, { "owner.range", o.m_maxDistance }, { "owner.loop_start", o.m_nLoopStart }, { "owner.loop_end", o.m_nLoopEnd }, { "owner.decay", o.m_nDecay }, { "owner.ambient", o.m_bAmbient }, { "owner.custom", o.m_bCustom }, { "owner.audio_volume", o.m_audioVolume }, { "owner.sample", input.at("mapped") } });
			if (value("object")) {
				expected["owner.signature"] = o.m_linkType.obj.nObjSig;
				expected["owner.object"] = o.m_linkType.obj.nObject;
			} else expected.insert({ { "owner.segment", o.m_linkType.pos.nSegment }, { "owner.side", o.m_linkType.pos.nSide }, { "owner.x", o.m_linkType.pos.position.x }, { "owner.y", o.m_linkType.pos.position.y }, { "owner.z", o.m_linkType.pos.position.z } });
		}
		for (unsigned i = 0; i < audio.m_channels.storage.size(); ++i) {
			const auto &channel = audio.m_channels.storage[i];
			if (!channel.m_info.bPlaying) continue;
			++expected["voices"];
			const auto &request = audio.request;
			const auto priority = audio_channel_reference_priority(request.sound_class, request.volume, request.pan, request.loop, request.owner);
			expected.insert({ { "voice.class", priority.sound_class }, { "voice.persistent", priority.persistent }, { "voice.kind", request.owner >= 0 ? 1 : 2 } });
			if (request.sound != value("sample") || request.sound_class != (value("object") ? value("class") : 0)) throw std::runtime_error("Source requested another physical sample or class");
			expected.insert({ { "voice.sample", input.at("mapped") }, { "voice.owner", request.owner }, { "voice.remaining", request.loop ? -1 : request.end - request.start + 1 }, { "voice.volume", channel.m_info.nVolume }, { "voice.output_volume", outputs[i].volume }, { "voice.left", outputs[i].pan[0] }, { "voice.right", outputs[i].pan[1] } });
		}
		if (expected != native) {
			for (const auto &[key, actual] : native) {
				const auto found = expected.find(key);
				if (found == expected.end() || found->second != actual) throw std::runtime_error("Creation case " + std::to_string(cases) + " differs at " + key + ": native=" + std::to_string(actual) + " source=" + (found == expected.end() ? "missing" : std::to_string(found->second)));
			}
			throw std::runtime_error("Creation case " + std::to_string(cases) + " omitted source fields");
		}
		++cases;
	}
	if (tag != "end" || cases != 768 || ordinal != cases) throw std::runtime_error("Creation receipt coverage is incomplete");
	if (stream >> tag) throw std::runtime_error("Unexpected trailing creation receipt data");
	std::cout << cases << " native fresh sound creations match actual source; duplicate lookup, external owners and logical translation are separate\n";
}
