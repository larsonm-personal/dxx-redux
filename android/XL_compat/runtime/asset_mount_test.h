// Real Android catalog/mount code with reset spies in the native lifecycle target
#include "android_mission_assets.h"
#include "../scoped_sound.h"

static unsigned asset_reset_before_calls, asset_reset_baseline_calls;
static std::filesystem::path asset_remove_after_probe;
void xl_test_asset_mounts_probed(void)
{
	if (!asset_remove_after_probe.empty()) {
		std::filesystem::remove(asset_remove_after_probe);
		asset_remove_after_probe.clear();
	}
}
extern "C" void android_mission_asset_reset_before(void)
{
	++asset_reset_before_calls;
}
extern "C" void android_mission_asset_reset_baseline(void)
{
	++asset_reset_baseline_calls;
}

static void asset_mount_test(int argc, char **argv)
{
	const auto root = std::filesystem::path(find_arg_value(argc, argv, "-userdir")) / "mount-probe";
	const auto first = root / "first", second = root / "second";
	const auto tail = root / "tail";
	std::filesystem::create_directories(first);
	std::filesystem::create_directories(second);
	std::filesystem::create_directories(tail);
	const xl_compat::Bytes descriptor{ 'n', 'a', 'm', 'e', '=', 't', '\n' };
	write_bytes((first / "test.mn2").string().c_str(), descriptor);
	write_bytes((second / "test.mn2").string().c_str(), descriptor);
	write_bytes((first / "collision.txt").string().c_str(), { 'a' });
	write_bytes((second / "collision.txt").string().c_str(), { 'b' });
	write_bytes((first / "OWNED.WAV").string().c_str(), { 'a' });
	const auto sound_root = first / "mods/pilot";
	std::filesystem::create_directories(sound_root / "sounds/level01");
	std::filesystem::create_directories(sound_root / "sounds/slevel01");
	write_bytes((sound_root / "sounds/VOICE.WAV").string().c_str(), { 'm' });
	write_bytes((sound_root / "sounds/level01/VOICE.WAV").string().c_str(), { 'l' });
	write_bytes((sound_root / "sounds/slevel01/VOICE.WAV").string().c_str(), { 's' });
	const auto broken = root / "broken.hog";
	write_bytes(broken.string().c_str(), { 'B', 'A', 'D', '!' });
	const auto manifest = std::filesystem::path(PHYSFS_getWriteDir()) / ".mission_assets.json";
	require(!std::filesystem::exists(manifest), "private catalog fixture already exists");
	nlohmann::json entries = nlohmann::json::array();
	const auto entry = [&](const char *id, const std::vector<std::string> &mounts, const char *file = "test.mn2") {
		entries.push_back({ { "alias", std::string("missions/_packs/") + id + "/test.mn2" },
		                    { "key", id },
		                    { "owner", id },
		                    { "revision", "fixture" },
		                    { "descriptor", file },
		                    { "mounts", mounts } });
	};
	entry("first", { first.string() });
	entry("broken", { second.string(), broken.string(), tail.string() });
	entry("duplicate", { second.string(), second.string() });
	entry("overlap", { second.string(), first.string() });
	entry("conflict", { find_arg_value(argc, argv, "-extra-dir") }, "missions/obsrv-xl.mn2");
	std::ofstream output(manifest);
	output << nlohmann::json({ { "schema", 1 }, { "entries", entries } }).dump();
	output.close();
	require(bool(output), "cannot write private catalog fixture");
	struct Cleanup {
		std::filesystem::path manifest;
		~Cleanup()
		{
			android_mission_assets_free_begin();
			android_mission_assets_free_end();
			android_mission_assets_shutdown();
			std::error_code ignored;
			std::filesystem::remove(manifest, ignored);
		}
	} cleanup{ manifest };
	android_mission_assets_init();
	char resolved[PATH_MAX];
	require(android_mission_assets_prepare("missions/_packs/first/test.mn2", resolved, sizeof(resolved)) &&
	            android_mission_assets_activate(),
	        "cannot activate initial catalog context");
	require(asset_reset_before_calls && asset_reset_baseline_calls && !std::strcmp(android_mission_assets_owner(), "first"),
	        "catalog control did not exercise reset/activation");
	const auto paths = [] {
		std::vector<std::string> result;
		char **list = PHYSFS_getSearchPath();
		require(list != nullptr, "search path unavailable");
		for (char **p = list; *p; ++p) result.emplace_back(*p);
		PHYSFS_freeList(list);
		return result;
	};
	char owned_path[PATH_MAX];
	require(android_mission_assets_find_owned_file("owned.wav", owned_path, sizeof(owned_path)),
	        "case-correct owned lookup failed");
	PHYSFS_file *owned_file = PHYSFS_openRead(owned_path);
	char owned_byte = 0;
	require(owned_file && PHYSFS_readBytes(owned_file, &owned_byte, 1) == 1 && owned_byte == 'a', "owned lookup selected different content");
	PHYSFS_close(owned_file);
	const auto lookup = [](const std::string &path) {
		char resolved_path[PATH_MAX];
		return android_mission_assets_find_owned_file(path.c_str(), resolved_path, sizeof(resolved_path)) ? std::string(resolved_path) : std::string{};
	};
	require(xl_compat::resolve_scoped_sound("pilot", "voice", 1, 22050, lookup).scope == "level" &&
	            xl_compat::resolve_scoped_sound("pilot", "voice", -1, 22050, lookup).scope == "secret_level" &&
	            xl_compat::resolve_scoped_sound("pilot", "voice", 2, 22050, lookup).scope == "mission",
	        "owned sound scope selection failed");
	write_bytes((sound_root / "pilot.s22").string().c_str(), { 'b' });
	require(xl_compat::resolve_scoped_sound("pilot", "voice", 1, 22050, lookup).bank, "owned bank did not gate WAV selection");
	std::filesystem::remove(sound_root / "pilot.s22");
	for (const char *path : { "../OWNED.WAV", "/OWNED.WAV", "C:OWNED.WAV", "x\\OWNED.WAV", "./OWNED.WAV", "descent2.ham" })
		require(!android_mission_assets_find_owned_file(path, owned_path, sizeof(owned_path)), "foreign or unsafe resource accepted as mission owned");
	for (const char *id : { "broken", "duplicate", "conflict", "overlap" }) {
		const auto before_paths = paths();
		const auto before_reset = asset_reset_before_calls, before_baseline = asset_reset_baseline_calls;
		const auto generation = android_mission_assets_generation();
		const auto context = std::string(android_mission_assets_context_json());
		const auto alias = std::string("missions/_packs/") + id + "/test.mn2";
		require(!android_mission_assets_prepare(alias.c_str(), resolved, sizeof(resolved)), "invalid mount candidate accepted");
		require(!android_mission_assets_activate(), "rejected preparation authorized activation");
		require(paths() == before_paths && asset_reset_before_calls == before_reset &&
		            asset_reset_baseline_calls == before_baseline && android_mission_assets_generation() == generation &&
		            context == android_mission_assets_context_json(),
		        "failed mount probe changed active ownership, search order or reset state");
		PHYSFS_file *file = PHYSFS_openRead("collision.txt");
		char value = 0;
		require(file && PHYSFS_readBytes(file, &value, 1) == 1, "active resource unavailable after rejection");
		PHYSFS_close(file);
		require(value == 'a', "candidate resource leaked into game root");
	}
	xl_compat::Bytes archive{ 'D', 'H', 'F' };
	const char member[13] = "proof.txt";
	archive.insert(archive.end(), member, member + sizeof(member));
	archive.insert(archive.end(), { 1, 0, 0, 0, 'v' });
	write_bytes(broken.string().c_str(), archive);
	const auto before_failure_generation = android_mission_assets_generation();
	const auto before_failure_paths = paths();
	const auto before_failure_reset = asset_reset_before_calls;
	const auto before_failure_baseline = asset_reset_baseline_calls;
	asset_remove_after_probe = broken;
	require(!android_mission_assets_prepare("missions/_packs/broken/test.mn2", resolved, sizeof(resolved)),
	        "archive removed after probe accepted");
	require(!android_mission_assets_activate(), "failed acquisition authorized activation");
	require(!std::strcmp(android_mission_assets_owner(), "first") &&
	            paths() == before_failure_paths &&
	            asset_reset_before_calls == before_failure_reset &&
	            asset_reset_baseline_calls == before_failure_baseline &&
	            android_mission_assets_generation() == before_failure_generation &&
	            PHYSFS_getMountPoint(first.string().c_str()) &&
	            !PHYSFS_getMountPoint(second.string().c_str()) &&
	            !PHYSFS_getMountPoint(tail.string().c_str()) &&
	            !PHYSFS_getMountPoint(broken.string().c_str()),
	        "late acquisition failure changed active resources or reset state");
	const auto failure_resets = asset_reset_baseline_calls;
	require(!android_mission_assets_activate() && asset_reset_baseline_calls == failure_resets,
	        "failed transition could activate again without preparation");
	write_bytes(broken.string().c_str(), archive);
	require(android_mission_assets_prepare("missions/_packs/broken/test.mn2", resolved, sizeof(resolved)),
	        "repaired archive could not prepare");
	require(!android_mission_assets_prepare("missions/_packs/first/test.mn2", resolved, sizeof(resolved)),
	        "nested preparation replaced pending transition");
	require(PHYSFS_getMountPoint(broken.string().c_str()) && PHYSFS_getMountPoint(tail.string().c_str()),
	        "successful preparation did not retain acquired mounts");
	require(android_mission_assets_activate(), "acquired archives could not activate");
	require(!android_mission_assets_find_owned_file("owned.wav", owned_path, sizeof(owned_path)), "previous context remained visible to owned lookup");
	require(xl_compat::resolve_scoped_sound("pilot", "voice", 1, 22050, lookup).path.empty(), "scoped sound survived a context switch");
	require(android_mission_assets_find_owned_file("proof.txt", owned_path, sizeof(owned_path)), "owned archive member lookup failed");
	PHYSFS_file *proof = PHYSFS_openRead("proof.txt");
	char proof_value = 0;
	require(proof && PHYSFS_readBytes(proof, &proof_value, 1) == 1, "repaired archive payload unavailable");
	PHYSFS_close(proof);
	require(proof_value == 'v' && !std::strcmp(android_mission_assets_owner(), "broken"), "repaired archive ownership differs");
	require(android_mission_assets_prepare("missions/_packs/first/test.mn2", resolved, sizeof(resolved)) &&
	            android_mission_assets_activate(),
	        "existing owned mounts could not be reselected");
	PHYSFS_file *held = PHYSFS_openRead("collision.txt");
	require(held, "cannot retain active resource for reselection");
	require(android_mission_assets_prepare("missions/_packs/first/test.mn2", resolved, sizeof(resolved)) &&
	            android_mission_assets_activate(),
	        "same active context could not be reselected");
	char held_value = 0;
	require(PHYSFS_readBytes(held, &held_value, 1) == 1 && held_value == 'a', "reselection replaced the owned resource handle");
	PHYSFS_close(held);
	require(!android_mission_assets_activate(), "completed transition could be activated twice");
	std::cout << "Android acquisition failure: late archive loss preserves active ownership and reset state; fresh preparation recovers\n";
	std::cout << "Android mount preflight: corrupt archives, duplicate paths and foreign ownership rejected before reset; active context and reselection preserved\n";
}
