// Synthetic serialization controls; successful inspection never qualifies gameplay
void original_fire_translation(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	check(bool(input) && input.tellg() > 0, "Original fire archive is missing");
	const auto length = static_cast<std::uint64_t>(input.tellg());
	const auto read_at = [&](std::uint64_t offset, unsigned char *data, std::size_t size) {
		input.seekg(static_cast<std::streamoff>(offset));
		input.read(reinterpret_cast<char *>(data), static_cast<std::streamsize>(size));
		check(bool(input), "Original fire archive read failed");
	};
	const auto members = xl_compat::index_hog(length, read_at);
	const auto level = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "mvlev1.rl2"; });
	check(level != members.end(), "Original Pumo first fire level missing");
	check(level->size <= 64 * 1024 * 1024, "Original fire level exceeds reader bound");
	Bytes bytes(static_cast<std::size_t>(level->size));
	read_at(level->offset, bytes.data(), bytes.size());
	const auto translated = xl_compat::prepare_source_runtime_probe(bytes);
	unsigned count = 0;
	for (const auto &object : translated.source.objects)
		if (object.render == 11) {
			check(object.render_data.size() == 13 && object.render_data[11] == 3 && object.render_data[12] == 0, "Original Pumo opening particle identity/activation differs");
			if (!count) std::cout << "Original Pumo controlled fire face: object " << (&object - translated.source.objects.data()) << ", segment " << object.segment << ", side " << object.render_data[10] - 1 << '\n';
			++count;
		}
	check(count == 12, "Original Pumo opening fire count differs");
	unsigned actions = 0, enable = 0, disable = 0;
	for (const auto &action : translated.source.triggers) {
		unsigned targets = 0;
		for (const auto &target : action.targets)
			if (target[1] < 0 && target[0] >= 0 && std::size_t(target[0]) < translated.source.objects.size() && translated.source.objects[target[0]].render == 11) ++targets;
		if (!targets) continue;
		enable += action.type == 32;
		disable += action.type == 33;
		if (actions++ < 8) std::cout << "Original fire wall action " << (&action - translated.source.triggers.data()) << ": type " << action.type << ", flags " << action.flags << ", fire targets " << targets << '\n';
	}
	std::cout << "Original fire linked wall actions: " << actions << " (enable " << enable << ", disable " << disable << ")\n";
	const auto print_segment = [&](int index) {
		const auto &segment = translated.source.segments.at(index);
		auto low = translated.source.vertices.at(segment.vertices[0]), high = low;
		for (int vertex : segment.vertices)
			for (int axis = 0; axis < 3; ++axis) {
				low[axis] = std::min(low[axis], translated.source.vertices.at(vertex)[axis]);
				high[axis] = std::max(high[axis], translated.source.vertices.at(vertex)[axis]);
			}
		std::cout << "Original fire view segment " << index << " bounds";
		for (int axis = 0; axis < 3; ++axis) std::cout << ' ' << double(low[axis]) / 65536 << ':' << double(high[axis]) / 65536;
		std::cout << " children";
		for (int child : segment.children) std::cout << ' ' << child;
		std::cout << '\n';
	};
	print_segment(409);
	for (int child : translated.source.segments.at(409).children)
		if (child >= 0) print_segment(child);
	rejects([&] { xl_compat::prepare_source_runtime_probe(bytes); });
	std::cout << "Original Pumo fire translation: 12 disabled authored fire objects validated, source retained, normal strict-fire admission passed\n";
}

void effect_layout_contract(const char *directory = nullptr)
{
	if (directory) std::filesystem::create_directories(directory);
	int cases = 0, truncations = 0;
	for (int version : { 15, 17, 23, 25, 27 })
		for (int render : { 0, 8, 11, 12, 13 })
			for (int profile = 0; profile < 5; ++profile) {
				auto bytes = fixture(version);
				const auto base = xl_compat::inspect_level(bytes);
				const auto start = bytes.size();
				patch(bytes, base.game_offset + 35, static_cast<unsigned>(start));
				patch(bytes, base.game_offset + 39, 1);
				patch(bytes, base.game_offset + 43, 264);
				const int header = version >= 23 ? 80 : 79;
				const int id = profile == 1 ? 7 : profile == 2 ? 0
				                              : profile == 4   ? 2
				                              : render == 12   ? 1
				                              : render == 13   ? 2
				                              : render == 0    ? 3
				                                               : 0;
				Bytes object(header);
				object[0] = 20;
				object[1] = static_cast<unsigned char>(id);
				object[2] = profile == 2 ? 17 : 0;
				object[3] = profile == 3 ? 2 : 0;
				object[4] = static_cast<unsigned char>(render);
				const int shift = version >= 23;
				for (int i = 0; i < 9; ++i) patch(object, 20 + shift + i * 4, i % 4 == 0 ? 65536 : 0);
				patch(object, 56 + shift, 65536);
				patch(object, 60 + shift, 65536);
				object[76 + shift] = object[77 + shift] = 255;
				if (profile == 2)
					for (int value : { -19, 42, 131072 }) put(object, static_cast<unsigned>(value), 4);
				if (render == 11) {
					for (int value : { 3, 4, 5, 6, 7, 8 }) put(object, value, 4);
					for (int value : { 11, 22, 33, 44, 255 }) put(object, value, 1);
					if (version >= 18) put(object, 3, 1);
					if (version >= 19) put(object, profile % 2, 1);
				} else if (render == 12) {
					for (int value : { 12, 23, 34, 45, 56 }) put(object, value, 4);
					if (version > 22) put(object, static_cast<unsigned>(-19), 4);
					for (int value : { 7, 8, -9, 10, -11, 12 }) put(object, static_cast<unsigned>(value), 2);
					if (version > 21) put(object, 5, 1);
					for (int value = 13; value <= 24; ++value) put(object, value, 1);
					if (version >= 19) put(object, profile % 2, 1);
				} else if (render == 13) {
					std::array<unsigned char, 40> name{};
					if (profile == 0) std::memcpy(name.data(), "LOUD.WAV", 8);
					if (profile == 1)
						for (int i = 0; i < 40; ++i) name[i] = static_cast<unsigned char>('A' + i % 26);
					if (profile == 2) std::memcpy(name.data(), "Fx\0TRAILING.WAV", 15);
					if (profile == 3) name[0] = 255;
					object.insert(object.end(), name.begin(), name.end());
					put(object, static_cast<unsigned>(profile == 1 ? -1 : 10), 4);
					if (version >= 19) put(object, profile % 2, 1);
				}
				bytes.insert(bytes.end(), object.begin(), object.end());
				const auto decoded = xl_compat::inspect_level(bytes);
				check(decoded.objects.size() == 1, "effect object disappeared");
				const auto &out = decoded.objects[0];
				check(out.id == id && out.render == render && out.type == 20 && out.source_offset == start && out.source_end == bytes.size(), "effect source identity or serialized end lost");
				int read_id = profile == 2 ? 3 : id;
				if (render == 11) read_id = 0;
				if (render == 13) read_id = 2;
				const int read_render = render == 12 && read_id != 1 ? read_id == 2 ? 13 : 11 : render;
				check(out.reader_id == read_id && out.reader_render == read_render, "effect reference-reader identity normalization differs");
				check(out.control_data.size() == (profile == 2 ? 3u : 0u), "waypoint controller span differs");
				if (render == 11) check(out.render_data.size() == 13 && out.render_data[10] == 255 && out.render_data[11] == (version < 18 ? 0 : 3) && out.render_data[12] == (version < 19 ? 1 : profile % 2), "particle version defaults or fields differ");
				if (render == 12) check(out.render_data.size() == 26 && out.render_data[5] == (version <= 22 ? -1 : -19) && out.render_data[8] == -9 && out.render_data[12] == (version <= 21 ? 3 : 5) && out.render_data[25] == (version < 19 ? 1 : profile % 2), "lightning version defaults or fields differ");
				if (render == 13) {
					check(out.render_data.size() == 42 && out.render_data[40] == (profile == 1 ? -1 : 10) && out.render_data[41] == (version < 19 ? 1 : profile % 2), "sound version defaults or fields differ");
					if (profile == 1) check(out.render_data[39] != 0, "unterminated raw sound name was erased");
					const auto json = xl_compat::inventory_json(decoded);
					if (profile == 0) check(json.find("\"sound_filename_ascii\": \"loud.wav\"") != std::string::npos, "sound reader name normalization absent");
					if (profile == 2) check(json.find("\"sound_filename_ascii\": \"fx\"") != std::string::npos && out.render_data[3] == 'T', "sound name lost bytes after embedded terminator");
					if (profile == 3) check(json.find("sound_filename_ascii") == std::string::npos && out.render_data[0] == 255, "non-ASCII sound bytes were interpreted as a valid display name");
					if (profile == 4) check(json.find("\"sound_filename_ascii\": \"\"") != std::string::npos, "empty sound filename changed");
				}
				const bool sound_supported = render == 13 && profile == 0;
				check(xl_compat::runtime_blockers(decoded).empty() == sound_supported, "effect capability admission differs from implemented sound profile");
				rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
				if (render == 11 && version >= 23) {
					auto fire = bytes;
					patch(fire, start + header + 28, 0, 1);
					if (profile == 2 || profile == 3) {
						rejects([&] { xl_compat::prepare_source_runtime_probe(fire); });
					} else {
						const auto original = xl_compat::inspect_level(fire);
						const auto translated = xl_compat::prepare_source_runtime_probe(fire);
						check(xl_compat::runtime_blockers(original).empty(), "Normal fire admission rejected the supported shape");
						rejects([&] { xl_compat::prepare_gameplay_probe(fire); });
						const auto fire_report = xl_compat::compatibility_json("fire.rl2", fire);
						check(feature_row(fire_report, "effect_objects").find("\"status\": \"partial\"") != std::string::npos &&
						          fire_report.find("\"playable\": false") != std::string::npos,
						      "Strict fire metadata claimed mission playability or retained the unsupported effect gate");
						check(translated.source.objects[0].render_data == original.objects[0].render_data && translated.source.objects[0].id == id, "Fire translation changed authored payload or raw identity");
						const auto read32 = [&](std::size_t at) {
							unsigned value = 0;
							for (unsigned b = 0; b < 4; ++b) value |= unsigned(translated.stock_level.at(at + b)) << (b * 8);
							return value;
						};
						const auto object_at = read32(read32(12) + 35);
						Bytes expected(fire.begin() + start, fire.begin() + start + 6);
						expected.insert(expected.end(), fire.begin() + start + 7, fire.begin() + start + header);
						expected[1] = expected[4] = 0;
						check(expected.size() == 79 && object_at + expected.size() <= translated.stock_level.size() && std::equal(expected.begin(), expected.end(), translated.stock_level.begin() + object_at), "Fire native record lost audited header bytes or retained particle payload");
						for (int subtype : { 0, 1 }) {
							auto smoke = fire;
							patch(smoke, start + header + 29, subtype, 1);
							const auto translated_smoke = xl_compat::prepare_source_runtime_probe(smoke);
							// The shadow retains unused source records, including the distinct subtype
							const auto smoke_read32 = [&](std::size_t at) {
								unsigned value = 0;
								for (unsigned b = 0; b < 4; ++b) value |= unsigned(translated_smoke.stock_level.at(at + b)) << (b * 8);
								return value;
							};
							const auto smoke_object_at = smoke_read32(smoke_read32(12) + 35);
							check(translated_smoke.source.objects[0].render_data[11] == subtype && smoke_object_at + expected.size() <= translated_smoke.stock_level.size() && std::equal(expected.begin(), expected.end(), translated_smoke.stock_level.begin() + smoke_object_at), "Smoke/spray translation lost source subtype or changed the classic inert object header");
						}
						for (const auto field : { std::array<unsigned, 2>{ unsigned(header + 28), 7 }, { unsigned(header + 29), 2 }, { unsigned(header + 8), 11 }, { 6, 1 } }) {
							auto bad_fire = fire;
							patch(bad_fire, start + field[0], field[1], 1);
							rejects([&] { xl_compat::prepare_source_runtime_probe(bad_fire); });
						}
					}
				}
				const auto report = xl_compat::compatibility_json("effect.rl2", bytes);
				check(feature_row(report, "effect_objects").find(sound_supported ? "\"status\": \"partial\"" : "\"status\": \"blocked\"") != std::string::npos && report.find("\"playable\": false") != std::string::npos, "effect metadata implied complete gameplay support");
				for (std::size_t cut = header; cut < object.size(); ++cut) {
					auto truncated = bytes;
					truncated.resize(start + cut);
					rejects([&] { xl_compat::inspect_level(truncated); });
					++truncations;
				}
				for (const auto &field : std::array<std::array<int, 2>, 3>{ { { 3, 99 }, { 2, 18 }, { 4, 14 } } }) {
					auto unknown = bytes;
					unknown[start + field[0]] = static_cast<unsigned char>(field[1]);
					rejects([&] { xl_compat::inspect_level(unknown); });
				}
				if (directory) {
					const auto path = std::filesystem::path(directory) / ("e" + std::to_string(version) + "r" + std::to_string(render) + "p" + std::to_string(profile) + ".rl2");
					std::ofstream file(path, std::ios::binary);
					file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
					check(bool(file), "cannot write effect layout fixture");
				}
				++cases;
			}
	for (int version : { 15, 17, 23, 25, 27 })
		for (bool supported : { false, true }) {
			auto bytes = fixture(version);
			const auto base = xl_compat::inspect_level(bytes);
			const auto start = bytes.size();
			patch(bytes, base.game_offset + 35, static_cast<unsigned>(start));
			patch(bytes, base.game_offset + 39, 1);
			patch(bytes, base.game_offset + 43, 264);
			Bytes object(version >= 23 ? 80 : 79);
			object[0] = 19;
			object[2] = 13;
			object[4] = supported ? 5 : 0;
			const int shift = version >= 23;
			for (int i = 0; i < 9; ++i) patch(object, 20 + shift + i * 4, i % 4 == 0 ? 65536 : 0);
			patch(object, 56 + shift, 65536);
			patch(object, 60 + shift, 65536);
			object[76 + shift] = object[77 + shift] = 255;
			put(object, 128, 4);
			if (supported) {
				put(object, 0, 4);
				put(object, static_cast<unsigned>(-1), 4);
				put(object, 0, 1);
			}
			bytes.insert(bytes.end(), object.begin(), object.end());
			const auto decoded = xl_compat::inspect_level(bytes);
			check(xl_compat::runtime_blockers(decoded).empty() == supported, "static explosion capability differs from its shape");
			const auto report = xl_compat::compatibility_json("static.rl2", bytes);
			const auto row = feature_row(report, "effect_objects");
			check(row.find(supported ? "\"status\": \"partial\"" : "\"status\": \"blocked\"") != std::string::npos &&
			          row.find("1 static explosions") != std::string::npos && report.find("\"playable\": false") != std::string::npos,
			      "Static explosion metadata lost its count, shape blocker or resource-dependent qualification");
			if (!supported) check(row.find("static_explosion at record 0") != std::string::npos, "Static shape rejection appeared in the wrong browser row");
		}
	std::cout << "XL effect layout controls passed: " << cases << " profiles and " << truncations << " payload truncations; ten static-explosion browser profiles; unsupported effect requirements remain gated\n";
}
