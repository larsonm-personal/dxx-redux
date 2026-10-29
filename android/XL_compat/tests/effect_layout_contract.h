// Synthetic serialization controls; successful inspection never qualifies gameplay
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
				check(!xl_compat::runtime_blockers(decoded).empty(), "decoded effect gained runtime admission");
				rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
				const auto report = xl_compat::compatibility_json("effect.rl2", bytes);
				check(feature_row(report, "effect_objects").find("\"status\": \"blocked\"") != std::string::npos && report.find("\"playable\": false") != std::string::npos, "effect metadata implied gameplay support");
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
	std::cout << "XL effect layout controls passed: " << cases << " profiles and " << truncations << " payload truncations; gameplay remains gated\n";
}
