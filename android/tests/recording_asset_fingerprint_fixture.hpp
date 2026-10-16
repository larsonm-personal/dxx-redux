// Real manual recorder startup, then test-owned source mutation controls
#include "input_demo_assets.h"
static void test_recording_asset_fingerprints(bool imported)
{
	using json = nlohmann::ordered_json;
	const auto baseline = json::parse(input_demo_assets_capture());
	require(baseline["status"] == "complete", "actual loaded archives have complete fingerprints");
	require(json::parse(input_demo_assets_capture()) == baseline, "unchanged sources produce identical fingerprints");
#ifdef _WIN32
	char level[PATH_MAX];
	std::snprintf(level, sizeof(level), "%s", Level_names[Current_level_num - 1]);
	PHYSFSEXT_locateCorrectCase(level);
	const char *owner = PHYSFS_getRealDir(level);
	require(owner != nullptr, "loaded level has an archive owner");
	std::string archive_path = owner;
	const std::string archive_role = baseline["files"][0]["role"];
	for (char separator : { '\\', '/' }) {
		std::replace(archive_path.begin(), archive_path.end(), separator == '/' ? '\\' : '/', separator);
		input_demo_assets_note_archive(archive_role.c_str(), archive_path.c_str());
		require(json::parse(input_demo_assets_capture()) == baseline, "Windows archive separators preserve loaded source identity");
	}
#endif
	char error[256] = {};
	require(maybe_start_input_demo_recording(0), "start through the manual recording path");
	for (const auto &entry : baseline["files"])
		input_demo_recorder_note_asset(entry["role"].get<std::string>().c_str(), entry["sha256"].get<std::string>().c_str(), entry["size"].get<uint64_t>());
	input_demo_control_state controls = {};
	input_demo_control_pulse pulse = {};
	input_demo_result result;
	input_demo_capture_current_result(&result);
	result.frame_count = 1;
	unsigned seed = 0;
	d_rand_get_stream_state(D_RNG_SIM, &seed);
	require(input_demo_recorder_capture_frame(F1_0 / 25, &controls, &pulse, seed, 1, d_rand_get_call_count(), nullptr, nullptr, error, sizeof(error)), error);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	require(input_demo_recorder_flush_with_result("manual-assets.dximdemo", &result, error, sizeof(error)), error);
	input_demo_recorder_cancel();
	input_demo_file recorded;
	std::string failure;
	require(input_demo_file_read("manual-assets.dximdemo", &recorded, &failure), failure.c_str());
	require(json::parse(recorded.metadata.asset_fingerprints) == baseline, "manual demo captures exact loaded sources");
	require(input_demo_replay_load("manual-assets.dximdemo", error, sizeof(error)), error);
	require(input_demo_assets_check_replay(error, sizeof(error)), "matching source preflight passes");
	std::string header;
	require(input_demo_metadata_to_header_line(recorded.metadata, &header, &failure), failure.c_str());
	auto duplicate = json::parse(header);
	duplicate["asset_fingerprints"]["files"][1] = duplicate["asset_fingerprints"]["files"][0];
	input_demo_metadata invalid;
	require(!input_demo_metadata_parse_header_line(duplicate.dump(), &invalid, &failure), "duplicate roles fail closed");
#ifdef DXX_BUILD_DESCENT_II
	const char *role = imported ? "d1_pig" : "d2_pig";
#else
	(void) imported;
	const char *role = "d1_pig";
#endif
	std::string pig;
	for (const auto &entry : baseline["files"])
		if (entry["role"] == role) pig = entry["name"];
	PHYSFS_file *input = PHYSFS_openRead(pig.c_str());
	require(input != nullptr, "open actual selected PIG for test-owned renamed copy");
	std::vector<unsigned char> bytes(PHYSFS_fileLength(input));
	require(PHYSFS_read(input, bytes.data(), 1, bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_close(input), "copy actual PIG without modifying it");
	write_fixture("fingerprint-control.pig", bytes);
	PHYSFS_file *renamed = PHYSFS_openRead("fingerprint-control.pig");
	require(renamed && PHYSFS_seek(renamed, 7), "open renamed identical PIG at a nonzero cursor");
	input_demo_assets_note_file(role, "fingerprint-control.pig", renamed);
	require(PHYSFS_tell(renamed) == 7 && PHYSFS_close(renamed), "hashing preserves the loader cursor");
	require(input_demo_assets_check_replay(error, sizeof(error)), "renamed identical source remains compatible");
	require(PHYSFS_mkdir("fingerprint-shadow"), "create isolated shadow directory");
	write_fixture("fingerprint-shadow/fingerprint-control.pig", bytes);
	require(PHYSFS_mount("fingerprint-shadow", nullptr, 0), "shadow with identical bytes from a different owner");
	require(json::parse(input_demo_assets_capture())["status"] == "unknown", "changed source ownership is unverified even with identical names");
	require(PHYSFS_unmount("fingerprint-shadow"), "remove isolated shadow mount");
	require(input_demo_assets_check_replay(error, sizeof(error)), "original ownership restored");
	bytes.back() ^= 1;
	write_fixture("fingerprint-control.pig", bytes);
	require(json::parse(input_demo_assets_capture())["status"] == "unknown" && !input_demo_assets_check_replay(error, sizeof(error)), "changed source cannot attest resident bytes");
	bytes.back() ^= 1;
	write_fixture("fingerprint-control.pig", bytes);
	require(input_demo_assets_check_replay(error, sizeof(error)), "restored exact source bytes are valid");
	input_demo_replay_unload();
	// A -> B -> A during an active manual recording remains explicitly unknown
	require(maybe_start_input_demo_recording(0), "start source transition recording");
	for (int change = 0; change < 2; ++change) {
		bytes.back() ^= 1;
		write_fixture("fingerprint-control.pig", bytes);
		renamed = PHYSFS_openRead("fingerprint-control.pig");
		input_demo_assets_note_file(role, "fingerprint-control.pig", renamed);
		PHYSFS_close(renamed);
	}
	input_demo_capture_current_result(&result);
	result.frame_count = 1;
	d_rand_get_stream_state(D_RNG_SIM, &seed);
	require(input_demo_recorder_capture_frame(F1_0 / 25, &controls, &pulse, seed, 1, d_rand_get_call_count(), nullptr, nullptr, error, sizeof(error)), error);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	require(input_demo_recorder_flush_with_result("changed-assets.dximdemo", &result, error, sizeof(error)), error);
	require(input_demo_file_read("changed-assets.dximdemo", &recorded, &failure), failure.c_str());
	require(json::parse(recorded.metadata.asset_fingerprints)["status"] == "unknown", "transient source change cannot recover a complete claim");
	require(input_demo_replay_load("manual-assets.dximdemo", error, sizeof(error)), error);
	require(PHYSFS_delete("fingerprint-control.pig"), "remove only the test-owned source copy");
	require(json::parse(input_demo_assets_capture())["status"] == "unknown" && !input_demo_assets_check_replay(error, sizeof(error)), "missing source cannot attest resident bytes");
	input_demo_replay_unload();
	const auto report = baseline.dump(2) + "\n";
	write_fixture("asset-fingerprints.json", std::vector<unsigned char>(report.begin(), report.end()));
	// A real custom mission HOG is included using its exact successful mount
	const std::string level_name = Level_names[0];
	input = PHYSFSX_openReadBuffered(level_name.c_str());
	require(input != nullptr, "read a public level for a test-owned custom mission");
	std::vector<unsigned char> level_bytes(PHYSFS_fileLength(input));
	require(PHYSFS_read(input, level_bytes.data(), 1, level_bytes.size()) == static_cast<PHYSFS_sint64>(level_bytes.size()) && PHYSFS_close(input), "copy public level bytes");
	std::vector<unsigned char> hog(20, 0);
	std::memcpy(hog.data(), "DHF", 3);
	std::memcpy(hog.data() + 3, level_name.c_str(), level_name.size());
	set_int(hog, 16, level_bytes.size());
	hog.insert(hog.end(), level_bytes.begin(), level_bytes.end());
#ifdef DXX_BUILD_DESCENT_II
	const std::string member = imported ? pig : "d2demo.ham";
	input = PHYSFSX_openReadBuffered(member.c_str());
	require(input != nullptr, "read real member source for custom HOG");
	std::vector<unsigned char> member_bytes(PHYSFS_fileLength(input));
	require(PHYSFS_read(input, member_bytes.data(), 1, member_bytes.size()) == static_cast<PHYSFS_sint64>(member_bytes.size()) && PHYSFS_close(input), "read complete custom member source");
	const size_t offset = hog.size();
	hog.resize(offset + 17, 0);
	std::memcpy(hog.data() + offset, member.c_str(), member.size());
	set_int(hog, offset + 13, member_bytes.size());
	hog.insert(hog.end(), member_bytes.begin(), member_bytes.end());
#endif
	write_fixture("assets.hog", hog);
#ifdef DXX_BUILD_DESCENT_II
	char descriptor[16];
	std::strcpy(descriptor, imported ? "assets.msn" : "assets.mn2");
#else
	char descriptor[] = "assets.msn";
#endif
	const auto mission = std::string("name = Asset test\nnum_levels = 1\n") + level_name + "\n";
	write_fixture(descriptor, std::vector<unsigned char>(mission.begin(), mission.end()));
	input = PHYSFSX_openReadBuffered(pig.c_str());
	require(input != nullptr, "restore actual PIG ownership before mission change");
	input_demo_assets_note_file(role, pig.c_str(), input);
	PHYSFS_close(input);
	require(load_mission_from_current_dir(descriptor), "load test-owned custom mission");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const auto custom = json::parse(input_demo_assets_capture());
	require(custom["status"] == "complete", "custom mission sources complete");
	bool have_custom = false;
	for (const auto &entry : custom["files"])
		if (entry["role"] == "mission_hog" && entry["name"] == "assets.hog") have_custom = true;
	require(have_custom, "custom HOG selected by successful mount");
#ifdef DXX_BUILD_DESCENT_II
	bool have_member = false;
	for (const auto &entry : custom["files"])
		if (entry["role"] == (imported ? "d1_pig" : "d2_ham") && entry.contains("container") && entry["container"] == "mission_hog") have_member = true;
	require(have_member, "archive member fingerprint records its exact verified HOG owner");
#endif
	require(maybe_start_input_demo_recording(0), "manual custom recording start");
	input_demo_capture_current_result(&result);
	result.frame_count = 1;
	d_rand_get_stream_state(D_RNG_SIM, &seed);
	require(input_demo_recorder_capture_frame(F1_0 / 25, &controls, &pulse, seed, 1, d_rand_get_call_count(), nullptr, nullptr, error, sizeof(error)), error);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	require(input_demo_recorder_flush_with_result("manual-custom-assets.dximdemo", &result, error, sizeof(error)), error);
	require(input_demo_file_read("manual-custom-assets.dximdemo", &recorded, &failure), failure.c_str());
	require(json::parse(recorded.metadata.asset_fingerprints) == custom, "manual custom recording retains all actual archive fingerprints");
#ifdef DXX_BUILD_DESCENT_II
	char builtin[16];
	std::strcpy(builtin, imported ? "descent" : "d2demo");
#else
	char builtin[] = "";
#endif
	require(load_mission_by_name(builtin), "return from custom HOG to builtin mission");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const auto returned = json::parse(input_demo_assets_capture());
	for (const auto &entry : returned["files"]) require(entry["role"] != "mission_hog", "custom archive ownership is cleared on mission teardown");
#ifdef DXX_BUILD_DESCENT_II
	// Ordinary D2 can retain decoded HAM tables after their mission HOG is unmounted
	if (!imported) require(returned["status"] == "unknown", "unmounted resident HAM source cannot be attested by a newly visible loose file");
	else require(returned == baseline, "imported base reload restores exact source snapshot");
#else
	require(returned == baseline, "native base source snapshot survives mission teardown");
#endif
#ifdef DXX_BUILD_DESCENT_II
	if (!imported) {
		input_demo_assets_extra_ham_loaded();
		const auto extra = json::parse(input_demo_assets_capture());
		require(extra["status"] == "unknown" && extra["error"] == "additional mission HAM is not fingerprinted", "untracked extra HAM definitions cannot claim complete");
	}
#endif
	std::puts("Manual recording asset fingerprint controls passed");
}
