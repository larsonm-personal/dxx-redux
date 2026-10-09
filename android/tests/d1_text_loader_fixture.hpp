#ifndef DXX_BUILD_DESCENT_II
static void test_d1_text_loader(const char *trace_path)
{
	static_assert(N_TEXT_STRINGS == 621 && N_TEXT_STRINGS_MIN == 514, "preserve original D1 text domain");
	nlohmann::json trace = nlohmann::json::array();
	std::vector<std::string> fallback;
	for (const int count : { 514, 600, 621 }) {
		for (const bool newline : { false, true }) {
			std::vector<std::string> authored;
			std::string encoded;
			for (int i = 0; i < count; ++i) {
				const std::string line = i == 116 ? "SPREADFIRE" : "entry-" + std::to_string(i);
				authored.push_back(line);
				for (const unsigned char c : line) {
					// Inverse of the original TXB rotate/XOR/rotate transform
					const unsigned char rotated = static_cast<unsigned char>((c >> 1) | (c << 7)) ^ 0xd3;
					encoded.push_back(static_cast<char>((rotated >> 1) | (rotated << 7)));
				}
				if (i + 1 < count || newline) encoded.push_back('\n');
			}
			PHYSFS_file *file = PHYSFS_openWrite("descent.txb");
			require(file != nullptr, "create isolated D1 text asset");
			const auto written = PHYSFS_writeBytes(file, encoded.data(), encoded.size());
			const auto closed = PHYSFS_close(file);
			require(written == static_cast<PHYSFS_sint64>(encoded.size()) && closed, "write complete D1 TXB asset");
			load_text();
			for (int i = 0; i < N_TEXT_STRINGS; ++i) {
				require(Text_string[i] != nullptr, "all original text pointers initialized");
				const std::string actual = Text_string[i];
				if (i < count) {
					const std::string expected = i == 116 ? "SPREAD" : authored[i] + (i == 330 ? "\n<Ctrl-C> converts format\nIntel <-> PowerPC" : "");
					require(actual == expected, "preserve every authored text string including final line");
				} else if (count == N_TEXT_STRINGS_MIN && !newline) {
					fallback.push_back(actual);
				} else {
					require(actual == fallback[i - N_TEXT_STRINGS_MIN], "optional fallback text is independent of final newline");
				}
			}
			int reason = DUMP_DUPNAME;
			require(std::strcmp(NET_DUMP_STRINGS(reason), "Duplicate callsign") == 0, "duplicate callsign message remains outside legacy text domain");
			trace.push_back({ { "strings", count }, { "final_newline", newline }, { "last_authored", Text_string[count - 1] }, { "last_original", Text_string[N_TEXT_STRINGS - 1] } });
			free_text();
			require(PHYSFS_delete("descent.txb"), "remove owned D1 text asset");
		}
	}
	require(fallback.size() == 107 && fallback.front() == "done" && fallback.back() == "Robot painting with texture %d", "original optional text bank endpoints preserved");
	FILE *file = std::fopen(trace_path, "wb");
	require(file != nullptr, "open D1 loader trace");
	const auto text = trace.dump(2) + "\n";
	const auto written = std::fwrite(text.data(), 1, text.size(), file);
	const auto closed = std::fclose(file);
	require(written == text.size() && closed == 0, "write complete D1 loader trace");
	std::puts("PASS: 6 actual D1 text loader cases");
}
#endif
