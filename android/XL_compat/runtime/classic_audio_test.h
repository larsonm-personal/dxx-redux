// Preparation and admission controls; no claim of spatial or audible playback
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
	using xl_compat::prepare_classic_audio;
	const auto before = xl_compat::named_audio_snapshot();
	const char *name = piggy_sound_name(0);
	require(name && *name && prepare_classic_audio("scope23.rl2", 2, 44100).samples.empty(), "Classic audio baseline is not empty");
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
		require(!xl_level_supported_at(level, 0, ordinal) && std::strstr(xl_level_rejection(), "classic WAV replacements require their playback consumer") && xl_compat::named_audio_snapshot() == before,
		        "Unapplied classic replacements silently admitted or disturbed live named voices");
	}
	require(identities.size() == 4, "Distinct classic scopes share an identity");
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
	for (const auto &path : created) std::filesystem::remove(path);
	require(prepare_classic_audio("scope23.rl2", 2, 44100).samples.empty() && xl_level_supported_at("scope23.rl2", 0, 2) && xl_compat::named_audio_snapshot() == before,
	        "Classic rejection recovery retained assets or changed live playback");
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
		for (int rate : { 44100, 48000 }) {
			nlohmann::json receipt = nlohmann::json::object();
			std::set<std::string> paths;
			int selections = 0;
			for (int ordinal = 1; ordinal <= Last_level; ++ordinal) {
				const auto prepared = xl_compat::prepare_classic_audio(Level_names[ordinal - 1], ordinal, rate);
				for (const auto &sample : prepared.samples) paths.insert(sample.path);
				selections += int(prepared.samples.size());
				receipt[std::to_string(ordinal)] = classic_audio_receipt(prepared);
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
			std::cout << "Classic original preparation: " << int(Last_level) << " levels, " << selections << " selections, " << paths.size() << " WAVs at " << rate << "; playback unqualified\n";
		}
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Classic original audio failed: " << error.what() << '\n';
		return 1;
	}
}
