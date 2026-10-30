// Resource preparation and loaded replacement playback controls
#include "audio_geometry_test.h"
#include "classic_playback_test.h"
static nlohmann::json classic_audio_receipt(const xl_compat::ClassicAudioResources &resources)
{
	nlohmann::json result = nlohmann::json::array();
	for (const auto &sample : resources.samples) {
		std::string pcm;
		pcm.reserve(sample.pcm.samples.size() * 2);
		for (auto word : sample.pcm.samples) {
			pcm.push_back(char(std::uint16_t(word) & 255));
			pcm.push_back(char(std::uint16_t(word) >> 8));
		}
		result.push_back({ { "sample", sample.sample }, { "name", sample.name }, { "path", sample.path }, { "source_sha256", sample.source_hash }, { "pcm_sha256", picosha2::hash256_hex_string(pcm) }, { "rate", sample.pcm.rate }, { "channels", sample.pcm.source_channels }, { "bits", sample.pcm.source_bits }, { "frames", sample.pcm.samples.size() / sample.pcm.source_channels } });
	}
	return result;
}

static void classic_audio_controls(const std::filesystem::path &root)
{
	classic_audio_geometry_controls(root);
	using xl_compat::prepare_classic_audio;
	const auto before = xl_compat::named_audio_snapshot();
	const char *name = piggy_sound_name(0);
	require(name && *name && prepare_classic_audio("scope23.rl2", 2, 44100).samples.empty(), "Classic audio baseline is not empty");
	const auto baseline = xl_compat::prepare_classic_playback({});
	require(!baseline.samples.empty() && !baseline.identity.empty(), "Complete native bank is empty");
	unsigned native_samples = 0;
	for (const auto &sample : baseline.samples) {
		require(sample.sample == int(native_samples) && sample.name == piggy_sound_name(int(native_samples)) && sample.path == "native:" + sample.name && sample.pcm.rate == 44100 && sample.pcm.source_channels == 2 && sample.pcm.source_bits == 16 && !sample.pcm.samples.empty(), "Complete native bank omitted or misidentified a stock sample");
		++native_samples;
	}
	require(!piggy_sound_name(int(native_samples)), "Complete native bank omitted trailing samples");
	const auto native_before = GameSounds[0];
	for (int malformed : { 0, 1, 2, 3 }) {
		GameSounds[0] = native_before;
		if (malformed == 0) GameSounds[0].data = nullptr;
		if (malformed == 1) GameSounds[0].bits = 16;
		if (malformed == 2) GameSounds[0].freq = 12345;
		if (malformed == 3) GameSounds[0].length = 0;
		bool rejected = false;
		try {
			xl_compat::prepare_classic_playback({});
		} catch (const std::exception &) {
			rejected = true;
		}
		GameSounds[0] = native_before;
		require(rejected && xl_compat::named_audio_snapshot() == before, "Invalid native bank changed live playback or was accepted");
	}
	// A reused allocation and equal bytes at another sample rate must invalidate
	// the conversion cache; malformed inputs above must still fail on a hit
	std::vector<unsigned char> changed(native_before.data, native_before.data + native_before.length);
	changed[changed.size() / 2] ^= 1;
	GameSounds[0].data = changed.data();
	const auto changed_bytes = xl_compat::prepare_classic_playback({});
	require(changed_bytes.identity != baseline.identity && changed_bytes.samples[0].source_hash != baseline.samples[0].source_hash && changed_bytes.samples[0].pcm.samples != baseline.samples[0].pcm.samples, "Stock conversion cache ignored changed native bytes");
	changed[changed.size() / 2] ^= 1;
	const auto same_allocation = xl_compat::prepare_classic_playback({});
	require(same_allocation.identity == baseline.identity && same_allocation.samples[0].pcm.samples == baseline.samples[0].pcm.samples, "Stock conversion cache trusted an unchanged pointer instead of its bytes");
	GameSounds[0] = native_before;
	GameSounds[0].freq = native_before.freq == 11025 ? 22050 : 11025;
	const auto changed_rate = xl_compat::prepare_classic_playback({});
	require(changed_rate.identity != baseline.identity && changed_rate.samples[0].pcm.samples.size() != baseline.samples[0].pcm.samples.size(), "Stock conversion cache ignored the native sample rate");
	GameSounds[0] = native_before;
	const auto recovered = xl_compat::prepare_classic_playback({});
	require(recovered.identity == baseline.identity && recovered.samples[0].pcm.samples == baseline.samples[0].pcm.samples, "Stock conversion cache did not recover the original PCM");
	const std::string filename = std::string(name) + ".wav";
	const auto sound_root = root / "mods" / "xscope" / "sounds";
	std::vector<std::filesystem::path> created;
	const auto copy = [&](const std::filesystem::path &from, const std::filesystem::path &to) {
		std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
		created.push_back(to);
	};
	for (const std::string folder : { "", "level01", "level02", "slevel01" })
		copy(sound_root / folder / "scoped.wav", sound_root / folder / filename);
	std::set<std::string> identities;
	for (int ordinal : { 1, 2, -1, 3 }) {
		const auto level = ordinal == 3 ? "scope27.rl2" : "scope23.rl2";
		const std::string folder = ordinal == 1 ? "level01/" : ordinal == 2 ? "level02/"
		                                                   : ordinal == -1  ? "slevel01/"
		                                                                    : "";
		const auto prepared = prepare_classic_audio(level, ordinal, 44100);
		require(prepared.samples.size() == 1 && prepared.samples[0].sample == 0 && prepared.samples[0].name == name &&
		            prepared.samples[0].path == "mods/xscope/sounds/" + folder + filename,
		        "Classic audio selected a different sample or scope");
		const auto complete = xl_compat::prepare_classic_playback(prepared);
		require(complete.samples.size() == baseline.samples.size() && complete.identity != baseline.identity && complete.samples[0].path == prepared.samples[0].path && complete.samples[0].pcm.samples == prepared.samples[0].pcm.samples, "Complete bank did not install its selected replacement");
		for (std::size_t i = 1; i < complete.samples.size(); ++i)
			require(complete.samples[i].source_hash == baseline.samples[i].source_hash && complete.samples[i].pcm.samples == baseline.samples[i].pcm.samples, "Replacement changed an unrelated native sample");
		const auto &pcm = prepared.samples[0].pcm;
		require(pcm.source_channels == 2 && pcm.source_bits == 16 && pcm.rate == 44100 && pcm.samples.size() == 44100, "Classic preparation lost stereo source format");
		const int left = ordinal == 1 ? 2500 : ordinal == 2 ? -3000
		                                   : ordinal == -1  ? 11000
		                                                    : 8000;
		const int right = ordinal == 1 ? -7500 : ordinal == 2 ? 9000
		                                     : ordinal == -1  ? -5000
		                                                      : -4000;
		for (std::size_t i = 0; i < pcm.samples.size(); ++i) require(pcm.samples[i] == (i % 2 ? right : left), "Classic prepared PCM differs from selected scope");
		identities.insert(prepared.identity);
		require(xl_level_supported_at(level, 0, ordinal) && xl_compat::named_audio_snapshot() == before,
		        "Valid replacement preflight failed or disturbed live named voices");
	}
	require(identities.size() == 4, "Distinct classic scopes share an identity");
	// Repeated partial-bank preparation must validate and fingerprint native members
	const auto partial = prepare_classic_audio("scope23.rl2", 2, 44100);
	const auto partial_baseline = xl_compat::prepare_classic_playback(partial);
	const auto partial_native = GameSounds[1];
	for (int malformed : { 0, 1, 2, 3 }) {
		GameSounds[1] = partial_native;
		if (malformed == 0) GameSounds[1].data = nullptr;
		if (malformed == 1) GameSounds[1].bits = 16;
		if (malformed == 2) GameSounds[1].freq = 12345;
		if (malformed == 3) GameSounds[1].length = 0;
		bool rejected = false;
		try {
			xl_compat::prepare_classic_playback(partial);
		} catch (const std::exception &) {
			rejected = true;
		}
		GameSounds[1] = partial_native;
		require(rejected && xl_compat::named_audio_snapshot() == before, "Repeated partial-bank preparation accepted malformed native data or changed live playback");
	}
	std::vector<unsigned char> partial_bytes(partial_native.data, partial_native.data + partial_native.length);
	GameSounds[1].data = partial_bytes.data();
	partial_bytes[partial_bytes.size() / 2] ^= 1;
	const auto changed_partial = xl_compat::prepare_classic_playback(partial);
	require(changed_partial.identity != partial_baseline.identity && changed_partial.samples[1].pcm.samples != partial_baseline.samples[1].pcm.samples &&
	            changed_partial.samples[0].pcm.samples == partial_baseline.samples[0].pcm.samples,
	        "Repeated partial-bank preparation ignored changed native bytes or damaged its replacement");
	partial_bytes[partial_bytes.size() / 2] ^= 1;
	require(xl_compat::prepare_classic_playback(partial).identity == partial_baseline.identity, "Partial-bank preparation trusted an unchanged pointer instead of its bytes");
	GameSounds[1] = partial_native;
	GameSounds[1].freq = partial_native.freq == 11025 ? 22050 : 11025;
	const auto changed_partial_rate = xl_compat::prepare_classic_playback(partial);
	GameSounds[1] = partial_native;
	require(changed_partial_rate.identity != partial_baseline.identity && changed_partial_rate.samples[1].pcm.samples.size() != partial_baseline.samples[1].pcm.samples.size(),
	        "Repeated partial-bank preparation ignored a changed native rate");
	require(xl_compat::prepare_classic_playback(partial).identity == partial_baseline.identity && xl_compat::named_audio_snapshot() == before,
	        "Partial bank did not recover its original content or disturbed playback");
	const auto preferred = sound_root / "level02" / filename;
	auto uppercase = filename;
	SDL_strupr(uppercase.data());
	const auto upper_path = preferred.parent_path() / uppercase;
	const auto temporary = preferred.string() + ".renamed";
	std::filesystem::rename(preferred, temporary);
	std::filesystem::rename(temporary, upper_path);
	const auto case_corrected = prepare_classic_audio("scope23.rl2", 2, 44100);
	require(case_corrected.samples.size() == 1 && !SDL_strcasecmp(case_corrected.samples[0].path.c_str(), ("mods/xscope/sounds/level02/" + uppercase).c_str()) &&
	            case_corrected.samples[0].pcm.samples[0] == -3000 && case_corrected.samples[0].pcm.samples[1] == 9000,
	        "Classic folder inventory lost a differently cased preferred replacement");
	std::filesystem::rename(upper_path, temporary);
	std::filesystem::rename(temporary, preferred);
	write_bytes(preferred.string().c_str(), { 'b', 'a', 'd' });
	require(!xl_level_supported_at("scope23.rl2", 0, 2) && std::strstr(xl_level_rejection(), "Invalid XL classic sound"), "Malformed classic preferred resource fell back");
	std::filesystem::copy_file(sound_root / "level02" / "scoped.wav", preferred, std::filesystem::copy_options::overwrite_existing);
	const auto foreign = root / "classic-foreign";
	const auto shadow = foreign / "mods" / "xscope" / "sounds" / "level02" / filename;
	std::filesystem::create_directories(shadow.parent_path());
	std::filesystem::copy_file(preferred, shadow, std::filesystem::copy_options::overwrite_existing);
	require(PHYSFS_addToSearchPath(foreign.string().c_str(), 0), "Cannot mount classic foreign control");
	const bool rejected = !xl_level_supported_at("scope23.rl2", 0, 2) && std::strstr(xl_level_rejection(), "shadowed outside the selected mission sound folders");
	require(PHYSFS_removeFromSearchPath(foreign.string().c_str()) && rejected, "Foreign classic resource was accepted");
	const auto custom = root / "mods" / "xscope" / "xscope.s22";
	write_bytes(custom.string().c_str(), { 'b', 'a', 'd' });
	require(!xl_level_supported_at("scope23.rl2", 0, 2) && std::strstr(xl_level_rejection(), "sound bank suppresses external WAV replacements"), "Unsupported selected bank did not suppress WAVs");
	require(prepare_classic_audio("scope23.rl2", 2, 11025).samples.size() == 1, "Wrong-rate custom bank suppressed WAVs");
	std::filesystem::remove(custom);
	char entry[] = "classic-entry.sav", description[] = "XL before replacements";
	require(state_save_all_sub(entry, description), "Cannot preserve original playback before replacement controls");
	xl_classic_playback_control::run(root, filename);
	for (const auto &path : created) std::filesystem::remove(path);
	require(state_restore_all_sub(entry, 0) && xl_compat::named_audio_snapshot() == before, "Removing replacements did not restore the original bank and playback");
	require(prepare_classic_audio("scope23.rl2", 2, 44100).samples.empty() && xl_level_supported_at("scope23.rl2", 0, 2) && xl_compat::named_audio_snapshot() == before,
	        "Classic rejection recovery retained assets or changed live playback");
	require(xl_compat::prepare_classic_playback({}).identity == baseline.identity, "Complete stock bank identity changed after rejection recovery");
	std::cout << "Classic complete bank passed: " << native_samples << " stock samples, four scoped replacements, unchanged unrelated PCM and atomic malformed-native rejection\n";
	std::cout << "Classic audio preparation passed: four source scopes, stereo PCM, bank suppression, atomic malformed/foreign/unsupported rejection and cleanup\n";
}

static int classic_audio_original(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission(find_arg_value(argc, argv, "-classic-audio-original"), error, sizeof(error)), error);
		const auto expected = [&](const char *argument) {
			const char *value = find_arg_value(argc, argv, argument);
			require(value && std::atoi(value) > 0, "Classic original audit requires exact positive counts");
			return std::atoi(value);
		};
		require(Last_level == expected("-audio-levels") && Last_secret_level == 0, "Classic original normal-level membership differs");
		const int waves = expected("-audio-waves"), count = expected("-audio-selections");
		const char *repeat_argument = find_arg_value(argc, argv, "-audio-preparation-iterations");
		const int iterations = repeat_argument ? std::atoi(repeat_argument) : 1;
		require(iterations > 0 && iterations <= 100, "Original bank preparation iterations must be between 1 and 100");
		for (int rate : { 44100, 48000 }) {
			std::uint32_t preparation_ms = 0;
			nlohmann::json receipt = nlohmann::json::object();
			nlohmann::json playback = nlohmann::json::object();
			std::set<std::string> paths;
			int selections = 0;
			for (int ordinal = 1; ordinal <= Last_level; ++ordinal) {
				const auto prepared = xl_compat::prepare_classic_audio(Level_names[ordinal - 1], ordinal, rate);
				for (const auto &sample : prepared.samples) paths.insert(sample.path);
				selections += int(prepared.samples.size());
				receipt[std::to_string(ordinal)] = classic_audio_receipt(prepared);
				const auto started = SDL_GetTicks();
				auto complete = xl_compat::prepare_classic_playback(prepared);
				for (int iteration = 1; iteration < iterations; ++iteration) {
					auto repeated = xl_compat::prepare_classic_playback(prepared);
					require(repeated.identity == complete.identity && repeated.samples.size() == complete.samples.size(), "Repeated bank preparation changed identity or membership");
					complete = std::move(repeated);
				}
				preparation_ms += SDL_GetTicks() - started;
				auto &outputs = playback[std::to_string(ordinal)] = nlohmann::json::array();
				for (const auto &replacement : prepared.samples) {
					const auto &selected = complete.samples.at(unsigned(replacement.sample));
					require(selected.path == replacement.path && selected.source_hash == replacement.source_hash,
					        "Original complete bank did not install the selected replacement");
					const auto converted = xl_compat::prepare_audio_sample(selected.pcm, unsigned(rate));
					std::string bytes;
					bytes.reserve(converted.stereo.size() * 2);
					for (auto word : converted.stereo) {
						bytes.push_back(char(std::uint16_t(word) & 255));
						bytes.push_back(char(std::uint16_t(word) >> 8));
					}
					outputs.push_back({ { "sample", selected.sample }, { "path", selected.path }, { "frames", converted.frames() }, { "pcm_sha256", picosha2::hash256_hex_string(bytes) } });
				}
			}
			require(int(paths.size()) == waves && selections == count, "Classic original selected WAV membership differs");
			const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / ("classic-resources-" + std::to_string(rate) + ".json");
			const auto text = receipt.dump(2) + "\n";
			std::ifstream previous(output, std::ios::binary);
			const std::string old{ std::istreambuf_iterator<char>(previous), std::istreambuf_iterator<char>() };
			previous.close();
			if (old != text) {
				std::ofstream out(output, std::ios::binary);
				out << text;
				require(bool(out), "Cannot write classic original receipt");
			}
			const auto playback_output = output.parent_path() / ("classic-playback-" + std::to_string(rate) + ".json");
			const auto playback_text = playback.dump(2) + "\n";
			std::ifstream previous_playback(playback_output, std::ios::binary);
			const std::string old_playback{ std::istreambuf_iterator<char>(previous_playback), std::istreambuf_iterator<char>() };
			previous_playback.close();
			if (old_playback != playback_text) {
				std::ofstream out(playback_output, std::ios::binary);
				out << playback_text;
				require(bool(out), "Cannot write original prepared playback receipt");
			}
			std::cout << "Classic original preparation: " << int(Last_level) << " levels, " << selections << " selections, " << paths.size() << " WAVs at " << rate << "; complete bank and converted PCM witnessed, original level playback unqualified\n";
			if (repeat_argument) std::cout << "Classic bank preparation timing: " << Last_level * iterations << " calls, " << preparation_ms << " ms at " << rate << '\n';
		}
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Classic original audio failed: " << error.what() << '\n';
		return 1;
	}
}
