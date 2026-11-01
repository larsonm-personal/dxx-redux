#include "audio_resource.h"
#include "resource_validation.h"
#include "../named_sound.h"
#include "../wave.h"

static void addon_audio_resource_native_case(const std::filesystem::path &root, const std::filesystem::path &installation)
{
	using namespace xl_compat;
	require(std::filesystem::equivalent(installation, root / "installation"), "Addon audio control requires an isolated startup installation");
	const auto mission = std::filesystem::path(mission_resource_archive()).stem().string();
	const auto paths = named_sound_paths(mission, "08:lightning.wav", 1);
	const std::string addon = "sounds/d2x-xl/lightning.wav";
	const auto installed = installation / addon, mission_file = root / paths.back(), level_file = root / paths[1];
	const auto foreign = root / "foreign-addon-audio", shadow = foreign / addon;
	struct Cleanup {
		std::vector<std::filesystem::path> paths;
		std::string mount;
		~Cleanup()
		{
			if (!mount.empty()) PHYSFS_removeFromSearchPath(mount.c_str());
			for (const auto &path : paths) {
				std::error_code error;
				std::filesystem::remove(path, error);
			}
		}
	} cleanup;
	for (const auto &path : { installed, mission_file, level_file, shadow }) {
		require(!std::filesystem::exists(path), "Addon audio fixture would overwrite an existing resource");
		cleanup.paths.push_back(path);
		std::filesystem::create_directories(path.parent_path());
	}
	const auto wave = [](int sample) {
		Bytes bytes(48, 0);
		const auto put = [&](unsigned at, unsigned value, unsigned count) { for (unsigned i = 0; i < count; ++i) bytes[at+i] = static_cast<std::uint8_t>(value >> (8*i)); };
		std::memcpy(bytes.data(), "RIFF", 4);
		std::memcpy(bytes.data() + 8, "WAVEfmt ", 8);
		std::memcpy(bytes.data() + 36, "data", 4);
		put(4, 40, 4);
		put(16, 16, 4);
		put(20, 1, 2);
		put(22, 1, 2);
		put(24, 22050, 4);
		put(28, 44100, 4);
		put(32, 2, 2);
		put(34, 16, 2);
		put(40, 4, 4);
		put(44, unsigned(sample), 2);
		return bytes;
	};
	const auto load = [&] { return load_audio_resource("08:lightning.wav", Level_names[0], 1); };
	const auto sample = [](const AudioResource &resource) { return decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource.bytes.data()), resource.bytes.size()).samples.at(0); };
	const auto failure = [&](const char *reason) {
		try {
			(void) sample(load());
		} catch (const std::exception &error) {
			require(std::strstr(error.what(), reason), error.what());
			return;
		}
		throw std::runtime_error("Addon audio accepted a missing, malformed or foreign resource");
	};
	failure("Missing XL sound");
	write_bytes(installed.string().c_str(), wave(1234));
	const char *origin = PHYSFS_getRealDir(addon.c_str());
	require(origin && xl_resource_owns_installation(origin) && !mission_owns_resource_directory(origin), "Addon audio did not select a distinct owned startup installation");
	const auto retained = load();
	require(retained.path == addon && sample(retained) == 1234, "Startup addon WAV did not decode");
	write_bytes(mission_file.string().c_str(), wave(2345));
	require(sample(load()) == 2345, "Mission sound did not override startup addon");
	write_bytes(level_file.string().c_str(), wave(3456));
	require(sample(load()) == 3456, "Level sound did not override mission sound");
	write_bytes(level_file.string().c_str(), { 0, 1, 2 });
	failure("WAV");
	std::filesystem::remove(level_file);
	std::filesystem::remove(mission_file);
	write_bytes(shadow.string().c_str(), wave(4567));
	cleanup.mount = foreign.string();
	require(PHYSFS_addToSearchPath(cleanup.mount.c_str(), 0), "Cannot mount foreign addon audio control");
	failure("shadowed outside");
	require(PHYSFS_removeFromSearchPath(cleanup.mount.c_str()), "Cannot unmount foreign addon audio control");
	cleanup.mount.clear();
	std::filesystem::remove(installed);
	failure("Missing XL sound");
	require(sample(retained) == 1234, "Failed addon audio preparation changed retained PCM source bytes");
	std::cout << "XL addon audio resources: startup origin, level/mission precedence, malformed/foreign rejection and retained original bytes passed\n";
}
