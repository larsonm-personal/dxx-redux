// Test-only mutation at the native preflight/commit boundary
#include "no_damage_persistence_test.h"
#include "mission_prepare.h"
#include "input_demo_world_trace.h"
#include "input_demo_object_trace.h"
#include "prepared_ham.h"
#include "retained_resource.h"
extern "C" {
#include "gauges.h"
#include "endlevel.h"
}
static std::string descriptor_mutation_path;
static std::string archive_removal_path;
static std::string archive_corruption_path;
static int identity_allocation_failure;

static void descriptor_prepared_ham_controls(const std::filesystem::path &root)
{
	PHYSFS_file *source = PHYSFS_openRead("descent2.ham");
	require(source != nullptr, "missing authentic native HAM for destination control");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(source)));
	require(PHYSFS_readBytes(source, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "cannot retain HAM control bytes");
	PHYSFS_close(source);
	const auto directory = root / "preview-user";
	write_bytes((directory / "prepham.ham").string().c_str(), bytes);
	{
		std::ofstream descriptor(directory / "prepham.mn2");
		descriptor << "d2x-name = Prepared HAM control\nnum_levels = 1\nd2leva-1.rl2\n";
		require(bool(descriptor), "cannot write prepared HAM descriptor control");
	}
	auto *mission = mission_prepare_from_current_dir("prepham.mn2");
	require(mission != nullptr, "cannot prepare HAM control descriptor");
	struct Discard {
		mission_preparation **mission;
		~Discard()
		{
			mission_discard_prepared(mission);
		}
	} discard{ &mission };
	const auto original = gameplay_definitions();
	const auto live_mission = Current_mission;
	uint32_t sim_before;
	d_rand_get_state(&sim_before);
	const auto sim_calls = d_rand_get_call_count();
	const auto lookup_state = [] {
		std::vector<std::string> paths, formats;
		char **search = PHYSFS_getSearchPath();
		for (char **entry = search; entry && *entry; ++entry) paths.emplace_back(*entry);
		PHYSFS_freeList(search);
		const auto **types = PHYSFS_supportedArchiveTypes();
		for (const auto **entry = types; entry && *entry; ++entry) formats.emplace_back((*entry)->extension);
		return std::make_pair(paths, formats);
	};
	const auto lookup_before = lookup_state();
	const auto energy = Weapon_info[CONCUSSION_ID].energy_usage;
	struct Restore {
		fix energy;
		~Restore()
		{
			Weapon_info[CONCUSSION_ID].energy_usage = energy;
		}
	} restore{ energy };
	Weapon_info[CONCUSSION_ID].energy_usage ^= F1_0;
	const auto outgoing = gameplay_definitions();
	const auto live_ship = only_player_ship;
	const int live_cockpits = Num_cockpits, live_marker = Marker_model_num;
	{
		XlPreparedHam target(mission, "prepham.ham");
		const auto &tables = target.tables();
		require(*tables.N_weapon_types == N_weapon_types &&
		            tables.Weapon_info[CONCUSSION_ID].energy_usage == energy &&
		            tables.Weapon_info[CONCUSSION_ID].energy_usage != Weapon_info[CONCUSSION_ID].energy_usage,
		        "incoming HAM borrowed outgoing equal-count weapon definitions");
		require(*tables.N_polygon_models == N_polygon_models && *tables.NumTextures == NumTextures &&
		            *tables.Num_reactors == Num_reactors && *tables.Num_cockpits == Num_cockpits &&
		            *tables.Marker_model_num == Marker_model_num &&
		            tables.only_player_ship->model_num == only_player_ship.model_num &&
		            tables.Gauges[0].index == Gauges[0].index && tables.Gauges_hires[0].index == Gauges_hires[0].index &&
		            tables.cockpit_bitmap[0].index == cockpit_bitmap[0].index &&
		            *tables.exit_modelnum == N_polygon_models && *tables.destroyed_exit_modelnum == N_polygon_models,
		        "native HAM destination omitted ship, reactor, gauge, cockpit or exit definitions");
		for (int id = 0; id < N_polygon_models; ++id) {
			const auto &model = tables.Polygon_models[id];
			require(model.model_data != Polygon_models[id].model_data && model.model_data_size == Polygon_models[id].model_data_size &&
			            !std::memcmp(model.model_data, Polygon_models[id].model_data, model.model_data_size),
			        "prepared HAM model payload is not independently owned");
		}
		const auto *models = tables.Polygon_models;
		XlPreparedHam moved(std::move(target));
		require(moved.tables().Polygon_models == models && moved.tables().Weapon_info[CONCUSSION_ID].energy_usage == energy,
		        "moving candidate invalidated destination or model views");
		write_bytes((directory / "prepham.ham").string().c_str(), { 'b', 'a', 'd' });
		bool rejected = false;
		try {
			XlPreparedHam invalid(mission, "prepham.ham");
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected && moved.tables().Weapon_info[CONCUSSION_ID].energy_usage == energy &&
		            moved.tables().Polygon_models == models && gameplay_definitions() == outgoing,
		        "malformed incoming HAM changed live or retained candidate definitions");
	}
	require(gameplay_definitions() == outgoing && Current_mission == live_mission &&
	            !std::memcmp(&live_ship, &only_player_ship, sizeof(live_ship)) &&
	            Num_cockpits == live_cockpits && Marker_model_num == live_marker,
	        "discarding prepared HAM published target definitions");
	Weapon_info[CONCUSSION_ID].energy_usage = energy;
	write_bytes((directory / "prepham.ham").string().c_str(), bytes);
	{
		auto replacement = Robot_info[0];
		replacement.mass += 1;
		PHYSFS_file *output = PHYSFS_openWrite("prepham.hxm");
		require(output != nullptr, "cannot write incoming HXM control");
		PHYSFS_writeSLE32(output, 0x21584d48);
		PHYSFS_writeSLE32(output, 1);
		PHYSFS_writeSLE32(output, 1);
		PHYSFS_writeSLE32(output, 0);
		require(robot_info_write_n(&replacement, 1, output), "cannot encode incoming HXM robot");
		for (int section = 0; section < 4; ++section) PHYSFS_writeSLE32(output, 0);
		require(PHYSFS_close(output), "cannot close incoming HXM control");
		XlPreparedResources candidate{ XlPreparedHam(mission, "prepham.ham") };
		{
			struct RestoreCount {
				int count = N_robot_types;
				~RestoreCount()
				{
					N_robot_types = count;
				}
			} restore_count;
			N_robot_types = 0;
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(mission_prepared_open(mission, "prepham.hxm"), PHYSFS_close);
			candidate.prepare_hxm(input.get());
			require(N_robot_types == 0 && candidate.dependencies().robot_data[0].mass == replacement.mass,
			        "incoming HXM used or repaired outgoing table counts");
		}
		write_bytes((directory / "prepham.hxm").string().c_str(), { 'b', 'a', 'd' });
		bool rejected = false;
		try {
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> invalid(mission_prepared_open(mission, "prepham.hxm"), PHYSFS_close);
			candidate.prepare_hxm(invalid.get());
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected && candidate.dependencies().robot_data[0].mass == replacement.mass && gameplay_definitions() == original,
		        "malformed HXM changed retained target or live definitions");
		const auto original_robot = Robot_info[0];
		candidate.install_hxm();
		require(Robot_info[0].mass == replacement.mass, "incoming HXM was not published from retained definitions");
		reset_level_robots_file();
		require(Robot_info[0].mass == replacement.mass,
		        "mission HXM publication was incorrectly retired as a temporary level replacement");
		Robot_info[0] = original_robot;
	}
	{
		XlPreparedHam incoming(mission, "prepham.ham");
		const auto *payload = incoming.tables().Polygon_models[0].model_data;
		Weapon_info[CONCUSSION_ID].energy_usage ^= F1_0;
		XlPreparedResources resources(std::move(incoming));
		resources.prepare_banks("groupa.256");
		auto &graph = resources.dependencies();
		require(graph.resource_banks && graph.resource_banks->bitmap_count() > 1 && graph.resource_banks->sound_count() > 0,
		        "Incoming graph omitted its retained bank owners");
		const int bitmap = graph.texture_data[0].index;
		int logical = -1, sample = -1;
		for (int id = 0; id < MAX_SOUNDS; ++id) {
			const int resolved = digi_xlat_sound_from_tables(id, graph.low_memory, graph.sound_mappings, graph.alternate_sounds);
			if (id != 255 && resolved >= 0 && resolved != 255 && resolved < graph.resource_banks->sound_count()) {
				logical = id;
				sample = resolved;
				break;
			}
		}
		require(logical >= 0, "No mapped target sample for independent bank control");
		{
			struct RestoreBanks {
				int count = Num_bitmap_files;
				int sample;
				digi_sound sound;
				~RestoreBanks()
				{
					Num_bitmap_files = count;
					GameSounds[sample] = sound;
				}
			} restore_banks{ Num_bitmap_files, sample, GameSounds[sample] };
			Num_bitmap_files = 0;
			GameSounds[sample].data = nullptr;
			graph.bitmap(bitmap);
			graph.sound(logical);
			require(graph.bitmaps.count(bitmap) && graph.sounds.count(sample) &&
			            Num_bitmap_files == 0 && !GameSounds[sample].data,
			        "Target dependency walk paged or repaired outgoing native banks");
		}
		const auto bitmap_hash = graph.resource_banks->bitmap_hash(), sound_hash = graph.resource_banks->sound_hash();
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> pig(PHYSFS_openRead("groupa.pig"), PHYSFS_close);
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> snd(PHYSFS_openRead(graph.resource_banks->sample_rate() == 22050 ? "descent2.s22" : "descent2.s11"), PHYSFS_close);
		const auto pig_bytes = xl_retain_resource(pig.get()), sound_bytes = xl_retain_resource(snd.get());
		for (bool bad_bitmap : { false, true }) {
			auto bitmap_copy = pig_bytes, sound_copy = sound_bytes;
			(bad_bitmap ? bitmap_copy : sound_copy).resize(11);
			bool rejected = false;
			try {
				XlResourceBanks invalid(std::move(bitmap_copy), std::move(sound_copy), graph.resource_banks->sample_rate());
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			require(rejected && graph.resource_banks->bitmap_hash() == bitmap_hash && graph.resource_banks->sound_hash() == sound_hash,
			        "Malformed bank changed the retained target or was accepted");
		}

		require(graph.weapon_count == N_weapon_types &&
		            graph.weapon_data[CONCUSSION_ID].energy_usage == energy &&
		            graph.weapon_data[CONCUSSION_ID].energy_usage != Weapon_info[CONCUSSION_ID].energy_usage &&
		            graph.model_data[0].model_data != Polygon_models[0].model_data,
		        "incoming resource graph borrowed outgoing equal-count definitions");
		resources.install_hxm();
		require(Polygon_models[0].model_data == payload && gameplay_definitions() == original,
		        "incoming resource publication did not transfer its decoded target definitions");
		graph.weapon(CONCUSSION_ID);
		require(graph.weapon_data[CONCUSSION_ID].energy_usage == Weapon_info[CONCUSSION_ID].energy_usage,
		        "published target differs from its retained dependency graph");
	}
	uint32_t sim_after;
	d_rand_get_state(&sim_after);
	require(gameplay_definitions() == original && sim_before == sim_after && sim_calls == d_rand_get_call_count() &&
	            lookup_state() == lookup_before,
	        "prepared HAM control changed live definitions, SIM RNG or resource lookup");
	std::cout << "Prepared native HAM: independent equal-count target, complete destinations, owned models, move and malformed discard passed\n";
	std::cout << "Incoming resource definitions: retained graph and native ownership transfer passed\n";
	std::cout << "Incoming resource banks: independent bitmap and sound validation, malformed discard passed\n";
}
extern "C" int xl_test_fail_identity_allocation(int allocation)
{
	if (identity_allocation_failure != allocation) return 0;
	identity_allocation_failure = 0;
	return 1;
}
extern "C" void xl_test_descriptor_staged(void)
{
	if (!archive_corruption_path.empty()) {
		write_bytes(archive_corruption_path.c_str(), { 'b', 'a', 'd' });
		archive_corruption_path.clear();
	}
	if (!archive_removal_path.empty()) {
		const auto path = std::move(archive_removal_path);
		archive_removal_path.clear();
		require(std::filesystem::remove(path), "late archive loss control failed");
	}
	if (descriptor_mutation_path.empty()) return;
	const auto path = std::move(descriptor_mutation_path);
	descriptor_mutation_path.clear();
	std::ofstream output(path, std::ios::trunc);
	output << "d2x-name = Mutated after preflight\nnum_levels = 999999999999\n";
	output.close();
	require(bool(output), "cannot mutate staged descriptor fixture");
}

// Native descriptor discovery without admitting unaudited XL gameplay
static void descriptor_retained_dependency_controls()
{
	const auto walk = [](xl_assets::ObjectDependencies &graph) {
		graph.robot(0);
		graph.weapon(CONCUSSION_ID);
		graph.powerup(POW_SHIELD_BOOST);
		graph.texture(0);
		graph.effect(0);
		graph.wall_clip(0);
	};
	const auto same = [](const auto &a, const auto &b) {
		return a.robot_profiles == b.robot_profiles && a.robots == b.robots && a.weapons == b.weapons &&
		       a.powerups == b.powerups && a.models == b.models && a.clips == b.clips &&
		       a.bitmaps == b.bitmaps && a.sounds == b.sounds && a.textures == b.textures &&
		       a.effects == b.effects && a.wall_clips == b.wall_clips;
	};
	XlPreparedResources retained("d2leva-1.rl2");
	auto expected = retained.dependencies();
	walk(expected);
	bool bound_valid = false, live_rejected = false;
	{
		struct Restore {
			int weapons = N_weapon_types, powerups = N_powerup_types, clips = Num_vclips, effects = Num_effects;
			int walls = Num_wall_anims, textures = NumTextures, bitmaps = N_ObjBitmaps, low_memory = GameArg.SysLowMem;
			weapon_info weapon = Weapon_info[CONCUSSION_ID];
			powerup_type_info powerup = Powerup_info[POW_SHIELD_BOOST];
			vclip clip = Vclip[Powerup_info[POW_SHIELD_BOOST].vclip_num];
			eclip effect = Effects[0];
			wclip wall = WallAnims[0];
			bitmap_index texture = Textures[0];
			tmap_info info = TmapInfo[0];
			std::array<ubyte, MAX_SOUNDS> sounds, alternates;
			Restore()
			{
				std::copy_n(Sounds, MAX_SOUNDS, sounds.begin());
				std::copy_n(AltSounds, MAX_SOUNDS, alternates.begin());
			}
			~Restore()
			{
				N_weapon_types = weapons;
				N_powerup_types = powerups;
				Num_vclips = clips;
				Num_effects = effects;
				Num_wall_anims = walls;
				NumTextures = textures;
				N_ObjBitmaps = bitmaps;
				GameArg.SysLowMem = low_memory;
				Weapon_info[CONCUSSION_ID] = weapon;
				Powerup_info[POW_SHIELD_BOOST] = powerup;
				Vclip[powerup.vclip_num] = clip;
				Effects[0] = effect;
				WallAnims[0] = wall;
				Textures[0] = texture;
				TmapInfo[0] = info;
				std::copy(sounds.begin(), sounds.end(), Sounds);
				std::copy(alternates.begin(), alternates.end(), AltSounds);
			}
		} restore;
		Weapon_info[CONCUSSION_ID].render_type = 127;
		Powerup_info[POW_SHIELD_BOOST].vclip_num = VCLIP_MAXNUM;
		Vclip[restore.powerup.vclip_num].num_frames = 0;
		Effects[0].vc.num_frames = 0;
		WallAnims[0].num_frames = 0;
		Textures[0].index = 0;
		TmapInfo[0].eclip_num = MAX_EFFECTS;
		std::fill_n(Sounds, MAX_SOUNDS, ubyte(255));
		std::fill_n(AltSounds, MAX_SOUNDS, ubyte(255));
		GameArg.SysLowMem = !restore.low_memory;
		N_weapon_types = N_powerup_types = Num_vclips = Num_effects = Num_wall_anims = NumTextures = N_ObjBitmaps = 0;
		XlPreparedResources prepared("d2leva-1.rl2");
		walk(prepared.dependencies());
		bound_valid = same(expected, prepared.dependencies()) && N_weapon_types == 0 && NumTextures == 0 &&
		              Weapon_info[CONCUSSION_ID].render_type == 127 && Sounds[0] == 255;
		try {
			xl_assets::ObjectDependencies live;
			live.weapon(CONCUSSION_ID);
		} catch (const std::runtime_error &) {
			live_rejected = true;
		}
	}
	require(bound_valid && live_rejected, "prepared dependency walk used or repaired the previous mission's definition tables");
	auto invalid = retained.dependencies();
	std::vector<weapon_info> weapons(invalid.weapon_data, invalid.weapon_data + invalid.weapon_count);
	weapons.at(CONCUSSION_ID).render_type = 127;
	invalid.weapon_data = weapons.data();
	bool rejected = false;
	try {
		invalid.weapon(CONCUSSION_ID);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected, "invalid retained weapon definition fell back to the valid live table");
	xl_assets::ObjectDependencies recovered;
	walk(recovered);
	require(same(expected, recovered), "retained definition controls failed to restore live table mappings and counts");
	std::cout << "Retained dependency definitions: weapons, powerups, animations, textures and sound mappings ignore previous live tables; invalid candidate rejects\n";
}

static int descriptor_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_level("d2leva-1.rl2") == 0, "stock baseline level failed");
		require(!xl_resource_texture_scope_enabled() && !*mission_resource_archive(), "stock mission enabled XL resource lookup");
		descriptor_retained_dependency_controls();
		descriptor_prepared_ham_controls(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")));
		// Supplemental assertions-enabled selection; canonical Observatory always continues
		if (!std::strcmp(find_arg_value(argc, argv, "-descriptor-test"), "ham-prepare")) return 0;
		const auto stock_definitions = gameplay_definitions();
		const auto stock_model = arena_model();
		const int stock_segments = Num_segments;
		xl_compat::Bytes original_17;
		for (int pass = 0; pass < 2; ++pass) {
			// Distinguish real base initialization from leaving the previous tables untouched
			Robot_info[0].strength ^= 1;
			require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
			require(!std::strcmp(Current_mission_longname, "Observatory XL"), "original XL title was not discovered");
			require(Current_mission->enhanced == MISSION_ENHANCED_XL, "XL descriptor identity lost");
			require(xl_resource_texture_scope_enabled() && *mission_resource_archive(), "parsed XL descriptor did not seal resource identity");
			require(Last_level == 1 && Last_secret_level == 0 && !std::strcmp(Level_names[0], "obsrv-xl.rl2"), "original level list differs");
			require(!Current_mission->alternate_ham_file && bm_compatibility_warning_count() == 0, "XL descriptor was treated as stock extra-HAM");
			require(gameplay_definitions() == stock_definitions && arena_model() == stock_model, "descriptor changed gameplay definitions or model payload");
			XlPreparedResources clean_base("d2leva-1.rl2");
			clean_base.dependencies().robot(0);
			for (const auto &entry : { std::pair<const char *, int>{ "obsrv-xl.rl2", 52176 }, { "obsrv-xl.pog", 26144 } }) {
				const char *resolved = PHYSFS_getRealDir(entry.first);
				require(resolved && std::filesystem::path(resolved).filename() == "obsrv-xl.hog", "original companion HOG not selected");
				PHYSFS_file *file = PHYSFS_openRead(entry.first);
				require(file != nullptr, "original archive member missing");
				const auto size = PHYSFS_fileLength(file);
				require(size == entry.second, "original archive member size differs");
				if (!pass && !std::strcmp(entry.first, "obsrv-xl.rl2")) {
					original_17.resize(static_cast<std::size_t>(size));
					require(PHYSFS_readBytes(file, original_17.data(), original_17.size()) == size, "cannot retain original 17/36 source");
				}
				PHYSFS_close(file);
			}
			const auto rng_calls = d_rand_get_stream_call_count(D_RNG_SIM);
			unsigned rng_before = 0, rng_after = 0;
			require(d_rand_get_stream_state(D_RNG_SIM, &rng_before), "SIM state unavailable");
			auto geometry = [] {
				return std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments) +
				       std::string(reinterpret_cast<const char *>(Vertices), sizeof(*Vertices) * Num_vertices);
			};
			const auto before_geometry = geometry();
			const auto before_time = GameTime64;
			for (const char *level : { "d2leva-1.rl2" }) {
				require(!xl_level_supported(level, 0), "descriptor context bypassed preflight");
				const std::string reason = xl_level_rejection();
				const char *expected = "d2x-name mission resources";
				require(reason.find(expected) != std::string::npos, "preflight omitted actionable feature reason");
				require(load_level(level) != 0 && reason == xl_level_rejection(), "loader and preflight descriptor reasons differ");
			}
			require(xl_level_supported("obsrv-xl.rl2", 0), xl_level_rejection());
			require(load_level("d2leva-1.rl2") != 0, "XL descriptor admitted even a stock-format level");
			require(d_rand_get_stream_state(D_RNG_SIM, &rng_after) && rng_after == rng_before &&
			            d_rand_get_stream_call_count(D_RNG_SIM) == rng_calls && GameTime64 == before_time,
			        "rejection changed SIM state, draw count or game time");
			require(Num_segments == stock_segments && geometry() == before_geometry, "rejection changed loaded geometry");
			require(!xl_preview_pilot_active() && !xl_preview_water_segment(187), "rejection activated an XL runtime profile");
			require(load_base_mission("d2", error, sizeof(error)), error);
			require(Current_mission->enhanced == 0 && load_level("d2leva-1.rl2") == 0, "stock recovery failed");
			require(!PHYSFS_getRealDir("obsrv-xl.rl2"), "XL companion archive leaked into stock mission");
			require(!xl_resource_texture_scope_enabled() && !*mission_resource_archive(), "XL resource policy leaked into stock recovery");
			require(gameplay_definitions() == stock_definitions, "stock definitions changed after XL descriptor");
		}
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		{
			// A classic custom mission owns the same replacement layout but must not activate XL
			PHYSFS_file *input = PHYSFS_openRead("d2leva-1.rl2");
			require(input && PHYSFS_fileLength(input) > 0 && PHYSFS_fileLength(input) < 64 * 1024 * 1024, "classic resource control missing");
			xl_compat::Bytes level(static_cast<std::size_t>(PHYSFS_fileLength(input)));
			require(PHYSFS_readBytes(input, level.data(), level.size()) == PHYSFS_sint64(level.size()), "classic control read failed");
			PHYSFS_close(input);
			xl_compat::Bytes hog{ 'D', 'H', 'F' };
			auto member = [&](const char *name, const xl_compat::Bytes &bytes) {
				char filename[13] = {};
				require(std::strlen(name) < sizeof(filename), "policy control member too long");
				std::strcpy(filename, name);
				hog.insert(hog.end(), filename, filename + sizeof(filename));
				for (unsigned shift = 0; shift < 32; shift += 8) hog.push_back(static_cast<unsigned char>(bytes.size() >> shift));
				hog.insert(hog.end(), bytes.begin(), bytes.end());
			};
			member("clspol.rl2", level);
			auto unsupported = original_17;
			const auto inventory = xl_compat::inspect_level(unsupported);
			const auto function_offset = inventory.game_offset - (inventory.vertices.size() + inventory.segments.size() * 6 + 910) * 13 - inventory.segments.size() * 8;
			unsupported.at(function_offset) = 12; // Unsupported blocked-segment semantics
			member("xltail.rl2", unsupported);
			write_bytes((root / "clspol.hog").string().c_str(), hog);
			const auto replacement = root / "mods/clspol/textures/pattern.tga";
			std::filesystem::create_directories(replacement.parent_path());
			xl_compat::Bytes tga(18 + 16, 0);
			tga[2] = 2;
			tga[12] = tga[14] = 2;
			tga[16] = 32;
			tga[17] = 8;
			for (unsigned i = 18; i < tga.size(); i += 4) {
				tga[i] = 128;
				tga[i + 1] = 88;
				tga[i + 2] = 120;
				tga[i + 3] = 255;
			}
			write_bytes(replacement.string().c_str(), tga);
			auto select = [&](bool secret_xl) {
				std::ofstream descriptor(root / "clspol.mn2");
				descriptor << "name = Classic policy control\nnum_levels = 1\nclspol.rl2\n";
				if (secret_xl) descriptor << "num_secrets = 1\nxltail.rl2,1\n";
				descriptor.close();
				require(load_base_mission("clspol", error, sizeof(error)), error);
				require(bool(xl_resource_texture_scope_enabled()) == secret_xl && *mission_resource_archive(),
				        "owned textures or unlisted XL member changed the selected mission policy");
				require(load_level("clspol.rl2") == 0 && !xl_preview_pilot_active(), "classic level activated XL runtime semantics");
			};
			select(false);
			select(true);
			require(load_level("xltail.rl2") != 0, "mixed mission admitted unsupported secret-level semantics");
			select(false);
			require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "classic policy recovery failed");
			std::cout << "Resource policy: classic custom decoy textures stay inactive; only listed owned XL secret enables mission scope; classic recovery passes\n";
		}
		const auto search_paths = [] {
			std::vector<std::string> result;
			char **paths = PHYSFS_getSearchPath();
			require(paths != nullptr, "cannot inspect descriptor search paths");
			for (char **path = paths; *path; ++path) result.emplace_back(*path);
			PHYSFS_freeList(paths);
			return result;
		};
		{
			const auto snapshot = [] {
				auto objects = nlohmann::json::object();
				for (int slot = 0; slot < object_capacity(); ++slot)
					if (Objects[slot].type != OBJ_NONE) objects[std::to_string(slot)] = input_demo_object_trace_snapshot(Objects[slot], slot);
				return nlohmann::json{ { "world", input_demo_world_trace_snapshot() }, { "pool", input_demo_object_pool_trace_snapshot() }, { "objects", objects } };
			};
			Mission *live = Current_mission;
			const auto before = snapshot();
			const auto paths = search_paths();
			const std::string live_archive = mission_resource_archive();
			auto *prepared = mission_prepare_by_name("obsrv-xl");
			const Mission *candidate = mission_prepared_descriptor(prepared);
			require(prepared && candidate && candidate->enhanced == MISSION_ENHANCED_XL &&
			            candidate->last_level == 1 && !std::strcmp(candidate->filename, "obsrv-xl") &&
			            !std::strcmp(mission_prepared_level(prepared, 1), "obsrv-xl.rl2") &&
			            !mission_prepared_level(prepared, 0) && !mission_prepared_level(prepared, 2) &&
			            !mission_prepared_level(prepared, INT_MIN) && !mission_prepared_level(prepared, INT_MAX),
			        "retained original descriptor lost identity or level bounds");
			const std::string archive = mission_prepared_archive(prepared);
			require(Current_mission == live && snapshot() == before && search_paths() == paths &&
			            !PHYSFS_getMountPoint(archive.c_str()) && !PHYSFS_getRealDir("obsrv-xl.rl2"),
			        "mission preparation published candidate state or archive lookup");
			PHYSFS_file *member = mission_prepared_open(prepared, "OBSRV-XL.RL2");
			xl_compat::Bytes bytes(original_17.size());
			require(member && PHYSFS_readBytes(member, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) &&
			            bytes == original_17 && Current_mission == live && snapshot() == before &&
			            !PHYSFS_getRealDir("obsrv-xl.rl2") && live_archive == mission_resource_archive(),
			        "isolated candidate reads changed live lookup, world, pool, objects or RNG");
			const auto private_paths = search_paths();
			for (const char *bad : { "", "/obsrv-xl.rl2", "../obsrv-xl.rl2", "folder/../obsrv-xl.rl2", "folder//obsrv-xl.rl2", "C:/obsrv-xl.rl2", "folder\\obsrv-xl.rl2" })
				require(!mission_prepared_open(prepared, bad), "candidate resource escaped its relative namespace");
			require(!mission_commit_prepared(&prepared) && prepared && !mission_discard_prepared(&prepared) && prepared &&
			            Current_mission == live && snapshot() == before && search_paths() == private_paths,
			        "busy candidate lease was consumed or published");
			require(PHYSFS_close(member) && mission_discard_prepared(&prepared) && !prepared &&
			            mission_discard_prepared(&prepared) && search_paths() == paths && snapshot() == before && Current_mission == live,
			        "discard did not release candidate ownership and restore the original search path");
			// Derived copies preserve the authentic input files while the late-change control runs
			const auto derived_descriptor = root.parent_path() / "prepreg.mn2", derived_archive = root.parent_path() / "prepreg.hog";
			std::filesystem::copy_file(root / "obsrv-xl.hog", derived_archive);
			std::filesystem::copy_file(root / "obsrv-xl.mn2", derived_descriptor);
			auto *independent = mission_prepare_by_name("obsrv-xl");
			prepared = mission_prepare_from_current_dir("prepreg.mn2");
			require(independent && prepared && mission_discard_prepared(&independent) && !independent &&
			            !std::strcmp(mission_prepared_descriptor(prepared)->filename, "prepreg"),
			        "discarding another candidate invalidated retained identity storage");
			const std::string retained = mission_prepared_descriptor_text(prepared);
			{
				std::ofstream changed(derived_descriptor, std::ios::trunc);
				changed << "d2x-name = Changed after prepare\nnum_levels = 999999999999\n";
				require(bool(changed), "cannot write derived descriptor mutation control");
			}
			member = mission_prepared_open(prepared, "obsrv-xl.rl2");
			require(member && PHYSFS_fileLength(member) == PHYSFS_sint64(original_17.size()) && PHYSFS_close(member) &&
			            Current_mission == live && snapshot() == before,
			        "retained candidate could not read its independent archive before publication");
			require(mission_commit_prepared(&prepared) && !prepared && Current_mission != live &&
			            !std::strcmp(Current_mission_filename, "prepreg") && Last_level == 1 &&
			            !std::strcmp(Level_names[0], "obsrv-xl.rl2") && retained == mission_descriptor_snapshot() &&
			            mission_owns_resource_archive(PHYSFS_getRealDir("obsrv-xl.rl2")),
			        "native commit reparsed changed descriptor bytes or lost retained archive ownership");
			require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && search_paths() == paths,
			        "retained mission commit did not recover the original stock resource scope");
			live = Current_mission;
			const auto recovered = snapshot();
			require(!mission_prepare_from_current_dir("prepreg.mn2") && Current_mission == live &&
			            snapshot() == recovered && search_paths() == paths,
			        "malformed descriptor preparation changed the recovered world");
			std::cout << "Retained mission preparation: isolated original reads, busy leases, snapshot commit and discard preserve live state\n";
		}
		auto fixture = [&](const char *name, const std::string &body, bool accepted) {
			std::ofstream out(root / (std::string(name) + ".mn2"), std::ios::binary);
			out << body;
			out.close();
			Mission *previous = Current_mission;
			const std::string previous_path = previous ? previous->path : "";
			const auto definitions = gameplay_definitions();
			const auto resource_policy = xl_resource_texture_scope_enabled();
			const std::string resource_archive = mission_resource_archive();
			const std::string descriptor = mission_descriptor_snapshot() ? mission_descriptor_snapshot() : "";
			const auto mounted_paths = search_paths();
			const char *origin = PHYSFS_getRealDir("obsrv-xl.rl2");
			const std::string archive = origin ? origin : "";
			require(bool(load_base_mission(name, error, sizeof(error))) == accepted, (std::string("descriptor fixture acceptance differs: ") + name).c_str());
			if (accepted) require(Current_mission->enhanced == MISSION_ENHANCED_XL && load_level("d2leva-1.rl2") != 0, "fixture lost XL runtime guard");
			else {
				origin = PHYSFS_getRealDir("obsrv-xl.rl2");
				require(Current_mission == previous && (!previous || previous_path == Current_mission->path) &&
				            definitions == gameplay_definitions() && archive == (origin ? origin : ""),
				        "malformed descriptor discarded previous mission or archive ownership");
				require(mounted_paths == search_paths(), "rejected descriptor changed search-path contents or order");
				require(resource_policy == xl_resource_texture_scope_enabled() && resource_archive == mission_resource_archive(), "rejected descriptor changed retained resource policy");
				require(descriptor == (mission_descriptor_snapshot() ? mission_descriptor_snapshot() : ""),
				        "rejected descriptor changed owned requirement snapshot");
				XlPreparedResources retained_base("d2leva-1.rl2");
				retained_base.dependencies().robot(0);
			}
		};
		require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
		{
			const auto archive_path = root / "xlarchive.hog";
			const char *body = "d2x-name = Archive control\nnum_levels = 1\nobsrv-xl.rl2\n";
			write_bytes(archive_path.string().c_str(), { 'b', 'a', 'd' });
			fixture("xlarchive", body, false);
			std::filesystem::copy_file(root / "obsrv-xl.hog", archive_path, std::filesystem::copy_options::overwrite_existing);
			PHYSFS_file *busy_previous = PHYSFS_openRead("obsrv-xl.rl2");
			require(busy_previous != nullptr, "cannot hold previous mission archive open");
			fixture("xlarchive", body, false);
			fixture("xlnohog", body, false);
			require(PHYSFS_close(busy_previous), "cannot release previous mission archive handle");
			std::cout << "Mission archive handoff: busy previous archive rejects replacement and loose-level selection without changing prior state\n";
			fixture("xlarchive", body, true);
			PHYSFS_file *retained_file = PHYSFS_openRead("obsrv-xl.rl2");
			require(retained_file != nullptr, "cannot retain archive handle across reselection");
			fixture("xlarchive", body, true); // Reselection must accept the mission's own mount
			char retained_header[4];
			require(PHYSFS_readBytes(retained_file, retained_header, sizeof(retained_header)) == sizeof(retained_header) &&
			            !std::memcmp(retained_header, original_17.data(), sizeof(retained_header)),
			        "reselection invalidated an open archive member");
			require(PHYSFS_close(retained_file), "cannot close retained archive member");
			const char *origin = PHYSFS_getRealDir("obsrv-xl.rl2");
			require(origin && std::filesystem::path(origin).filename() == "xlarchive.hog" && mission_owns_resource_archive(origin),
			        "repaired archive did not become the owned resource source");
			require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
			const auto path = archive_path.string();
			require(PHYSFS_mount(path.c_str(), "foreign-archive-control", 1), "cannot mount foreign archive control");
			fixture("xlarchive", body, false);
			require(PHYSFS_unmount(path.c_str()), "cannot remove foreign archive control");
			archive_corruption_path = path;
			fixture("xlarchive", body, false);
			require(archive_corruption_path.empty(), "late archive corruption control did not run");
			std::filesystem::copy_file(root / "obsrv-xl.hog", archive_path, std::filesystem::copy_options::overwrite_existing);
			archive_removal_path = path;
			fixture("xlarchive", body, false);
			require(archive_removal_path.empty(), "late archive loss control did not run");
			require(load_base_mission("obsrv-xl", error, sizeof(error)), "archive loss recovery failed");
			std::cout << "Mission archive preflight: corrupt and foreign-owned HOGs preserve prior state; repaired/reselected archives pass; late corruption/loss retain prior mission and search order\n";
		}
		for (int allocation : { 1, 2 }) {
			identity_allocation_failure = allocation;
			fixture("xlidentity", "d2x-name = Identity failure control\nnum_levels = 1\nd2leva-1.rl2\n", false);
			require(!identity_allocation_failure, "mission identity allocation fault was not exercised");
		}
		std::cout << "Mission identity preflight: object and path allocation failures retain prior mission, archive and resource baseline\n";
		fixture("xllate", "d2x-name = Invalid late level\nnum_levels = 2\nobsrv-xl.rl2\ntoolongfilename.rl2\n", false);
		require(PHYSFS_getRealDir("obsrv-xl.rl2") && Current_mission->enhanced == MISSION_ENHANCED_XL,
		        "late descriptor failure lost original Observatory archive");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "preflight control stock recovery failed");
		{
			// Resolve text after mounting while preserving every directive's order/fallback
			write_bytes((root.parent_path() / "xlfirst.tex").string().c_str(), { 'a' });
			write_bytes((root.parent_path() / "xlsecond.txb").string().c_str(), { 'b' });
			descriptor_mutation_path = (root / "xlstaged.mn2").string();
			fixture("xlstaged", "d2x-name = Staged snapshot\nnum_levels = 1\nd2leva-1.rl2\nbriefing = xlfirst\nbriefing = xlsecond\nbriefing = missing\nending = xlfirst\n", true);
			require(descriptor_mutation_path.empty() && Last_level == 1 && !std::strcmp(Level_names[0], "d2leva-1.rl2") &&
			            !std::strcmp(Briefing_text_filename, "xlsecond.txb") && !std::strcmp(Ending_text_filename, "xlfirst.tex"),
			        "committed descriptor differs from validated snapshot or text ordering");
			require(mission_descriptor_snapshot() && !std::strstr(mission_descriptor_snapshot(), "999999999999"),
			        "requirement snapshot reopened the replaced descriptor");
			std::ifstream input(root / "xlstaged.mn2");
			const std::string changed{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			require(changed.find("999999999999") != std::string::npos, "descriptor mutation control did not run");
			std::cout << "Descriptor commit uses owned validated fields despite source replacement; text order and fallback preserved\n";
		}
		// A stock enhanced-mission fallback must not try to decode this synthetic HAM
		{
			std::ofstream poison(root.parent_path() / "xlcase.ham", std::ios::binary);
			poison << "invalid HAM sentinel";
		}
		require(PHYSFS_getRealDir("xlcase.ham") != nullptr, "HAM negative control is not mounted");
		// The companion HOG must be inspectable without decoding even stock-named assets
		xl_compat::Bytes shadow{ 'D', 'H', 'F' };
		const char member[13] = "descent2.ham";
		shadow.insert(shadow.end(), member, member + sizeof(member));
		shadow.insert(shadow.end(), { 4, 0, 0, 0, 'B', 'A', 'D', '!' });
		write_bytes((root / "xlshadow.hog").string().c_str(), shadow);
		fixture("xlshadow", "d2x-name = Shadow assets\nnum_levels = 1\nd2leva-1.rl2\n", true);
		const char *shadow_dir = PHYSFS_getRealDir("descent2.ham");
		require(shadow_dir && std::filesystem::path(shadow_dir).filename() == "xlshadow.hog", "stock-name HAM negative control is not active");
		require(load_mission_ham() == 0 && gameplay_definitions() == stock_definitions && arena_model() == stock_model,
		        "descriptor selection decoded or freed shadowed gameplay resources");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "shadow HAM recovery failed");
		{
			const char graphics_member[13] = "groupa.pig";
			std::copy(graphics_member, graphics_member + sizeof(graphics_member), shadow.begin() + 3);
			write_bytes((root / "xlgfx.hog").string().c_str(), shadow);
			fixture("xlgfx", "d2x-name = Shadow graphics\nnum_levels = 1\nd2leva-1.rl2\n", true);
			require(std::strstr(xl_resource_base_rejection(), "groupa.pig"), "descriptor default graphics override was not explained");
			require(load_mission_ham() == 0 && gameplay_definitions() == stock_definitions && arena_model() == stock_model,
			        "descriptor initialization mutated resources before default graphics rejection");
			require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "descriptor graphics rejection did not recover");
		}
		fixture("xlcase", "  D2X-NaMe = Case test\ntype = normal\nnum_levels = 1\nd2leva-1.rl2\n", true);
		fixture("xlcrlf", "d2x-name = DOS lines\r\nnum_levels = 1\r\nd2leva-1.rl2\r\n", true);
		fixture("xlcr", "d2x-name = Mac lines\rnum_levels = 1\rd2leva-1.rl2\r", true);
		fixture("xlnul", std::string("d2x-name = Hidden bytes\nnum_levels = 1\nd2leva-1.rl2\n") + '\0' + "hidden = yes\n", false);
		fixture("xlmix", "name = Legacy title\nnum_levels = 1\nd2leva-1.rl2\nd2x-name = XL identity\n", false);
		fixture("xlback", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n  name = Legacy identity\n", false);
		fixture("xlff", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\fname = Legacy identity\n", false);
		fixture("xlvt", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\vzname = Legacy identity\n", false);
		fixture("xlvtcrit", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\v!ham = other.ham\n", false);
		fixture("xldup", "d2x-name = First\n  d2x-name = Second\nnum_levels = 1\nd2leva-1.rl2\n", false);
		const auto hidden_title = std::string("d2x-name = A") + std::string(80, ' ') + "B\nnum_levels = 1\nd2leva-1.rl2\n";
		fixture("xlhidden", hidden_title.c_str(), false);
		const auto truncated_title = std::string("d2x-name = A;") + std::string(PATH_MAX, 'x') + "\nnum_levels = 1\nd2leva-1.rl2\n";
		fixture("xltrunc", truncated_title.c_str(), false);
		fixture("xllong", "d2x-name = Title exceeding the twenty five character native limit\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlfile", "d2x-name = Long filename\nnum_levels = 1\ntoolongfilename.rl2\n", false);
		fixture("xlsecrt", "d2x-name = Secret mapping\nnum_levels = 2\nd2leva-1.rl2\nd2leva-2.rl2\nnum_secrets = 1\nd2leva-s.rl2,2\n", true);
		require(Last_level == 2 && Last_secret_level == -1 && Secret_level_table[0] == 2 &&
		            !std::strcmp(Secret_level_names[0], "d2leva-s.rl2"),
		        "XL secret mapping differs");
		fixture("xlbadsec", "d2x-name = Bad secret\nnum_levels = 1\nd2leva-1.rl2\nnum_secrets = 1\nd2leva-s.rl2,2\n", false);
		fixture("xlsuffix", "d2x-name-extra = Invalid\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlempty", "d2x-name = ; comment only\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlcount", "d2x-name = Invalid count\nnum_levels = 99999999999999\n", false);
		const auto oversized = std::string("d2x-name = Oversized descriptor\nnum_levels = 1\nd2leva-1.rl2\n") + std::string(1024 * 1024, ';');
		fixture("xlhuge", oversized.c_str(), false);
		fixture("xlcrit", "d2x-name = Critical\nnum_levels = 1\nd2leva-1.rl2\n  !ham = other.ham\n", false);
		fixture("xlhog", "d2x-name = Archive\nnum_levels = 1\nd2leva-1.rl2\nhog = other.hog\n", false);
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "malformed descriptor recovery failed");
		if (PHYSFSX_exists("descent.pig", 1)) {
			require(load_base_mission(D1_MISSION_FILENAME, error, sizeof(error)), error);
			new_player_config();
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(d1_in_d2_has_native_assets(), "D1 transition control did not install native assets");
			require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
			require(!d1_in_d2_has_native_assets() && gameplay_definitions() == stock_definitions && arena_model() == stock_model,
			        "D1 to XL descriptor transition did not restore D2 base definitions");
			XlPreparedResources clean_base("d2leva-1.rl2");
			clean_base.dependencies().robot(0);
			require(xl_level_supported("obsrv-xl.rl2", 0), "D1 transition failed supported Observatory preflight");
			require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "D1/descriptor stock recovery failed");
			std::cout << "D1 -> XL descriptor -> D2: default graphics transition, native definitions and recovery passed\n";
		} else std::cout << "D1 descriptor transition untested: optional D1 assets unavailable\n";
		std::cout << "Original Observatory descriptor: title, level list and companion archive resolved; feature preflight accepted; stock isolation and malformed descriptors passed\n";
		std::cout << "XL descriptor base initialization: stale robot definitions reset, owned baseline captured, extra-HAM dialect not invoked\n";
		std::cout << "Descriptor preflight: malformed candidates retain prior mission, archive and owned resource baseline\n";
		asset_mount_test(argc, argv);
		format17_layout_test(original_17, find_arg_value(argc, argv, "-extra-dir"));
		no_damage_persistence_test(original_17);
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "XL descriptor FAIL: %s\n", e.what());
		return 1;
	}
}
