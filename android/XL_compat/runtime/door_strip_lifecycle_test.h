#include "pog_resources.h"
#include "texture_pixels.h"
#include "wall_strip_replay_test.h"
extern "C" {
#include "fireball.h"
}

static void write_door_strip_fixture(const std::filesystem::path &root, bool asymmetric = false, int blastable = 0, bool mismatched_pair = false)
{
	const std::string mission = mismatched_pair ? "stripbad" : blastable == 2 ? "stripex"
	                                                       : blastable        ? "stripbl"
	                                                       : asymmetric       ? "stripp"
	                                                                          : "stripdr";
	std::ifstream input(root / "missions/close.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			member.bytes = xl_test::add_door_pairs(member.bytes, 1);
			auto level = xl_compat::inspect_level(member.bytes);
			if (blastable) {
				for (std::size_t i : { std::size_t(0), std::size_t(1), level.walls.size() - 2, level.walls.size() - 1 }) {
					member.bytes.at(level.sections[1].offset + i * 25 + 16) = WALL_BLASTABLE;
					member.bytes.at(level.sections[1].offset + i * 25 + 17) = 0;
				}
				level = xl_compat::inspect_level(member.bytes);
			}
			if (mismatched_pair) {
				member.bytes.at(level.sections[1].offset + 25 + 16) = WALL_DOOR;
				level = xl_compat::inspect_level(member.bytes);
			}
			// Derived controls remove unused editor clips that would conflict with the strip owner
			for (std::size_t i = 0; i < level.walls.size(); ++i)
				if (level.walls[i].type != WALL_DOOR && level.walls[i].type != WALL_BLASTABLE)
					member.bytes.at(level.sections[1].offset + i * 25 + 21) = 255;
			level = xl_compat::inspect_level(member.bytes);
			XlPreparedResources retained("neptunxl.rl2", &level, 1);
			const auto &graph = retained.dependencies();
			int first = level.walls[0].clip;
			if (blastable == 2) {
				first = -1;
				for (int candidate = 0; candidate < graph.wall_clip_count; ++candidate)
					if (graph.wall_clip_data[candidate].flags & WCF_EXPLODES) {
						first = candidate;
						break;
					}
				require(first >= 0, "No retained exploding clip for derived strip");
				for (std::size_t i : { std::size_t(0), std::size_t(1), level.walls.size() - 2, level.walls.size() - 1 })
					member.bytes.at(level.sections[1].offset + i * 25 + 21) = static_cast<unsigned char>(first);
			}
			std::vector<std::array<int, 2>> strips{ { first, 16 } };
			if (asymmetric) {
				std::set<int> aliases;
				const auto &a = graph.wall_clip_data[first];
				for (int frame = 0; frame < a.num_frames; ++frame) aliases.insert(graph.texture_data[a.frames[frame]].index);
				for (int candidate = 0; candidate < graph.wall_clip_count; ++candidate) {
					const auto &b = graph.wall_clip_data[candidate];
					if (candidate == first || b.num_frames < 2 || b.play_time == a.play_time || (b.flags & WCF_BLASTABLE)) continue;
					bool overlaps = false;
					for (int frame = 0; frame < b.num_frames; ++frame) overlaps |= aliases.count(graph.texture_data[b.frames[frame]].index) != 0;
					if (overlaps) continue;
					member.bytes.at(level.sections[1].offset + 25 + 21) = static_cast<unsigned char>(candidate);
					strips.push_back({ candidate, 8 });
					break;
				}
				require(strips.size() == 2, "No independent unequal clip for derived strip pair");
			}
			for (const auto &strip : strips) {
				const auto &clip = graph.wall_clip_data[strip[0]];
				const unsigned frames = unsigned(strip[1]);
				const char *name = graph.resource_banks->bitmap_name(graph.texture_data[clip.frames[0]].index);
				require(name != nullptr, "Derived strip has no retained first-frame name");
				const auto path = root / "mods" / mission / "textures" / (std::string(name) + ".tga");
				std::filesystem::create_directories(path.parent_path());
				xl_compat::Bytes tga(18 + 64 * frames * 4, 0);
				tga[2] = 2;
				tga[12] = 8;
				tga[14] = static_cast<unsigned char>(8 * frames);
				tga[16] = 32;
				tga[17] = 8;
				for (unsigned pixel = 0; pixel < 64 * frames; ++pixel) {
					// Raw CTGA decoding reverses bottom-origin rows, including strip order
					const unsigned frame = frames - 1 - pixel / 64, at = 18 + pixel * 4;
					tga[at] = 30;
					tga[at + 1] = 50;
					tga[at + 2] = static_cast<unsigned char>(frame + 20);
					tga[at + 3] = frame & 1 ? 0 : 255;
				}
				write_bytes(path.string().c_str(), tga);
			}
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Derived strip member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<uint8_t>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "missions" / (mission + ".hog")).string().c_str(), output);
	std::ofstream descriptor(root / "missions" / (mission + ".mn2"));
	descriptor << "d2x-name = Derived XL " << (blastable == 2 ? "explode strips" : blastable ? "blast strips"
	                                                                                         : "strip doors")
	           << "\nnum_levels = 1\nneptunxl.rl2\n";
}

static void blastable_strip_lifecycle(const std::filesystem::path &root)
{
	write_door_strip_fixture(root, false, true);
	char error[256]{};
	require(load_base_mission("stripbl", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const int clip = Walls[0].clip_num, independent = Num_walls - 2;
	require(Walls[0].type == WALL_BLASTABLE && !(WallAnims[clip].flags & WCF_EXPLODES) && xl_pog_door_frames(clip) == 16,
	        "Derived blastable strip was not loaded");
	for (int index : { 0, 1, independent, independent + 1 }) {
		auto &wall = Walls[index];
		wall.hps = WALL_HPS;
		auto &side = Segments[wall.segnum].sides[wall.sidenum];
		side.tmap_num = WallAnims[clip].frames[0];
		side.tmap_num2 = 0;
	}
	const auto phase = [&](int index) { return xl_door_phases_snapshot()[Walls[index].segnum][Walls[index].sidenum].get<int>(); };
	std::ofstream samples(root / "blastable-strip-samples.txt");
	const auto damage = [&](fix amount) {
		samples << amount << ' ' << Walls[0].hps << ' ' << Walls[1].hps << ' ' << phase(0) << ' ' << phase(1)
		        << ' ' << int(Walls[0].flags) << ' ' << int(Walls[1].flags) << ' ';
		wall_damage(&Segments[Walls[0].segnum], Walls[0].sidenum, amount);
		samples << Walls[0].hps << ' ' << Walls[1].hps << ' ' << phase(0) << ' ' << phase(1)
		        << ' ' << int(Walls[0].flags) << ' ' << int(Walls[1].flags) << '\n';
	};
	const auto cold_restore = [&] {
		const auto phases = xl_door_phases_snapshot();
		const std::vector<wall> walls(Walls, Walls + Num_walls);
		char save[] = "strip-blast.sav", description[] = "Derived blastable strip";
		stop_time();
		require(state_save_all_sub(save, description), "Blastable strip save failed");
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_door_phases_snapshot().empty(), "Blastable phase owner leaked into stock");
		require(state_restore_all_sub(save, 0) && xl_door_phases_snapshot() == phases &&
		            std::size_t(Num_walls) == walls.size() && !std::memcmp(Walls, walls.data(), walls.size() * sizeof(wall)),
		        "Blastable strip cold restore lost damage, flags or phases");
	};
	require(phase(0) == 0 && phase(1) == 0, "Blastable side incorrectly received door initialization");
	cold_restore();
	for (int frame = 0; frame < 15; ++frame) {
		// Exactly at a threshold remains on the previous phase; one fixed-point unit crosses it
		const fix boundary = WALL_HPS * (16 - frame) / 16;
		damage(Walls[0].hps - boundary);
		require(phase(0) == (frame ? -frame : 0), "Blastable strip changed at an inclusive threshold");
		damage(1);
		require(phase(0) == -(frame + 1) && phase(1) == phase(0) && phase(independent) == 0 &&
		            Walls[0].hps == Walls[1].hps && !(Walls[0].flags & WALL_BLASTED),
		        "Blastable strip damage lost count, reciprocal state or independence");
		const auto &wall = Walls[0];
		const int texture = Segments[wall.segnum].sides[wall.sidenum].tmap_num;
		auto *view = xl_door_side_bitmap(wall.segnum, wall.sidenum, texture, &GameBitmaps[Textures[texture].index]);
		require(texture == WallAnims[clip].frames[0] && view->bm_data[0] == (frame & 1 ? 0 : frame + 20) &&
		            view->bm_data[3] == (frame & 1 ? 0 : 255) &&
		            !(wall_is_doorway(&Segments[wall.segnum], wall.sidenum) & WID_FLY_FLAG),
		        "Blastable strip lost authored mapping, visible frame or collision");
		if (frame == 7) cold_restore();
	}
	damage(Walls[0].hps - WALL_HPS / 16 + 1);
	require(phase(0) == -16 && phase(1) == -16 && (Walls[0].flags & WALL_BLASTED) && (Walls[1].flags & WALL_BLASTED) &&
	            (wall_is_doorway(&Segments[Walls[0].segnum], Walls[0].sidenum) & WID_FLY_FLAG) && phase(independent) == 0,
	        "Blastable strip final frame or collision differs");
	cold_restore();
	damage(WALL_HPS);
	require(phase(0) == -16, "Destroyed strip accepted further damage");
	// Native wall_destroy excludes segment zero; use this pair's reciprocal face
	const int destroy_face = Walls[independent].segnum ? independent : independent + 1;
	require(Walls[destroy_face].segnum != 0, "Direct destruction control needs a nonzero segment");
	wall_destroy(&Segments[Walls[destroy_face].segnum], Walls[destroy_face].sidenum);
	require(phase(independent) == -16 && phase(independent + 1) == -16, "Direct strip destruction lost final phase");
	std::cout << "Derived blast strips: strict damage thresholds, independent faces, collision, direct destruction and three cold restores passed\n";
}

static void exploding_strip_lifecycle(const std::filesystem::path &root)
{
	write_door_strip_fixture(root, false, 2);
	char error[256]{};
	require(load_base_mission("stripex", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const int clip = Walls[0].clip_num, independent = Num_walls - 2;
	require((WallAnims[clip].flags & WCF_EXPLODES) && xl_pog_door_frames(clip) == 16, "Exploding strip was not loaded");
	for (int object = Highest_object_index; object >= 0; --object)
		if (Objects[object].type != OBJ_NONE && object != ConsoleObject - Objects) obj_delete(object);
	obj_relink(ConsoleObject - Objects, 10);
	compute_segment_center(&ConsoleObject->pos, &Segments[10]);
	ConsoleObject->size = F1_0 / 16;
	ConsoleObject->mtype.phys_info.velocity = {};
	for (int index : { 0, 1, independent, independent + 1 }) {
		auto &wall = Walls[index];
		wall.hps = WALL_HPS;
		auto &side = Segments[wall.segnum].sides[wall.sidenum];
		side.tmap_num = WallAnims[clip].frames[0];
		side.tmap_num2 = 0;
	}
	const auto phase = [&](int index) { return xl_door_phases_snapshot()[Walls[index].segnum][Walls[index].sidenum].get<int>(); };
	std::ofstream samples(root / "exploding-strip-samples.txt");
	const auto advance = [&] {
		const auto count = [] {
			int result = 0;
			for (int i = 0; i <= Highest_object_index; ++i) result += Objects[i].type == OBJ_FIREBALL;
			return result;
		};
		const int before = count();
		samples << FrameTime << ' ' << expl_wall_list[0].time << ' ' << phase(0) << ' ' << int(Walls[0].flags) << ' ';
		do_exploding_wall_frame();
		samples << expl_wall_list[0].time << ' ' << (expl_wall_list[0].segnum >= 0 ? 1 : 0) << ' ' << phase(0) << ' '
		        << int(Walls[0].flags) << ' ' << count() - before << '\n';
	};
	const auto cold_restore = [&] {
		const auto phases = xl_door_phases_snapshot();
		const std::vector<wall> walls(Walls, Walls + Num_walls);
		const std::vector<expl_wall> explosions(expl_wall_list, expl_wall_list + MAX_EXPLODING_WALLS);
		char save[] = "strip-explode.sav", description[] = "Exploding strip";
		stop_time();
		require(state_save_all_sub(save, description), "Exploding strip save failed");
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(state_restore_all_sub(save, 0) && xl_door_phases_snapshot() == phases &&
		            std::size_t(Num_walls) == walls.size() && !std::memcmp(Walls, walls.data(), walls.size() * sizeof(wall)) &&
		            !std::memcmp(expl_wall_list, explosions.data(), explosions.size() * sizeof(expl_wall)),
		        "Exploding strip cold restore lost native timers, walls or phase");
	};
	wall_damage(&Segments[Walls[0].segnum], Walls[0].sidenum, WALL_HPS);
	require(Walls[0].hps == -1 && !(Walls[0].flags & WALL_BLASTED) && phase(0) == 0 &&
	            expl_wall_list[0].segnum == Walls[0].segnum && expl_wall_list[0].time == 0,
	        "Strip damage did not register the native delayed explosion");
	FrameTime = F1_0 / 4;
	advance();
	require(expl_wall_list[0].time == F1_0 / 4 && phase(0) == 0 && phase(independent) == 0,
	        "Exploding strip changed its side phase too early");
	cold_restore();
	FrameTime = F1_0 / 2;
	advance();
	require(expl_wall_list[0].time == 3 * F1_0 / 4 && !(Walls[0].flags & WALL_BLASTED) && phase(0) == 0,
	        "Exploding strip passability boundary became inclusive");
	FrameTime = 1;
	advance();
	require((Walls[0].flags & WALL_BLASTED) && (Walls[1].flags & WALL_BLASTED) && phase(0) == -16 && phase(1) == -16 &&
	            (wall_is_doorway(&Segments[Walls[0].segnum], Walls[0].sidenum) & WID_FLY_FLAG) && phase(independent) == 0,
	        "Exploding strip lost final frame, reciprocal flags or independent wall");
	cold_restore();
	FrameTime = F1_0 / 4;
	advance();
	require(expl_wall_list[0].segnum == -1 && phase(0) == -16, "Exploding strip did not retire its native timer");
	cold_restore();
	wall_destroy(&Segments[Walls[0].segnum], Walls[0].sidenum);
	require(expl_wall_list[0].segnum == -1 && phase(0) == -16, "Destroyed strip restarted its explosion");
	write_door_strip_fixture(root, false, 1, true);
	const auto before = input_demo_world_trace_snapshot();
	Mission *live = Current_mission;
	auto *incoming = mission_prepare_by_name("stripbad");
	require(incoming != nullptr, "Cannot prepare mismatched strip descriptor");
	bool rejected = false;
	try {
		auto candidate = XlPreparedResources::prepare_level_inputs(incoming, 1);
	} catch (const std::exception &error) {
		rejected = std::string(error.what()).find("same clip on both reciprocal walls") != std::string::npos;
	}
	require(mission_discard_prepared(&incoming) && rejected && Current_mission == live && input_demo_world_trace_snapshot() == before && phase(0) == -16,
	        "Mismatched incoming strip wall was admitted or changed outgoing world");
	wall_damage(&Segments[Walls[independent].segnum], Walls[independent].sidenum, WALL_HPS);
	FrameTime = F1_0 / 4;
	do_exploding_wall_frame();
	wall_strip_recorded_future(root, "exploding");
	std::cout << "Derived explode strips: native damage and fireballs, strict passability boundary, retirement and three cold restores passed\n";
}

static void door_strip_restore_guards(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("strip-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture strip extension guards");
	std::ifstream input(root / "preview-user/strip-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	require(encoded.size() > 16, "Strip extension missing payload");
	const auto before = xl_door_phases_snapshot();
	const auto &wall = Walls[0];
	for (int variant = 0; variant < 9; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &phases = block["door_phases"];
		if (variant == 0) block.erase("door_phases");
		if (variant == 1) phases = nlohmann::json::object();
		if (variant == 2) phases.erase(phases.begin());
		if (variant == 3) phases[wall.segnum].push_back(0);
		if (variant == 4) phases[wall.segnum][wall.sidenum] = -17;
		if (variant == 5) phases[wall.segnum][wall.sidenum] = 3;
		if (variant == 6) phases[wall.segnum][wall.sidenum] = bool(0);
		if (variant == 7) phases[wall.segnum][wall.sidenum] = 0.5;
		if (variant == 8) block["source"] = block["source"].get<std::uint64_t>() ^ 1;
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
			bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((root / "preview-user/strip-invalid.bin").string().c_str(), bad);
		file = PHYSFS_openRead("strip-invalid.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_door_phases_snapshot() == before,
		        "Malformed strip extension changed live phases or was accepted");
	}
	file = PHYSFS_openRead("strip-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_door_phases_snapshot() == before,
	        "Valid strip extension failed after malformed controls");
}

static void door_strip_unequal_pair(const std::filesystem::path &root)
{
	write_door_strip_fixture(root, true);
	char error[256]{};
	require(load_base_mission("stripp", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const int a = Walls[0].clip_num, b = Walls[1].clip_num;
	require(a != b && xl_pog_door_frames(a) == 16 && xl_pog_door_frames(b) == 8, "Unequal strip pair lost retained counts");
	wall_open_door(&Segments[Walls[0].segnum], Walls[0].sidenum);
	FrameTime = F1_0 / 64;
	fix elapsed = 0;
	bool unequal = false, saved = false;
	for (int tick = 0; tick < 8192 && (Walls[0].state == WALL_DOOR_OPENING || Walls[1].state == WALL_DOOR_OPENING); ++tick) {
		elapsed += FrameTime;
		wall_frame_process();
		const auto phases = xl_door_phases_snapshot();
		for (int index : { 0, 1 }) {
			const auto &wall = Walls[index];
			const int frames = xl_pog_door_frames(wall.clip_num), raw = elapsed / (WallAnims[wall.clip_num].play_time / frames);
			require(phases[wall.segnum][wall.sidenum] == -((std::min) (raw, frames - 1) + 1) &&
			            bool(wall.flags & WALL_DOOR_OPENED) == (raw > frames / 2),
			        "Unequal strip face lost its own clock or collision threshold");
		}
		unequal |= phases[Walls[0].segnum][Walls[0].sidenum] != phases[Walls[1].segnum][Walls[1].sidenum];
		if (!saved && Walls[0].state != Walls[1].state) {
			require(Num_open_doors == 1 && xl_door_state_valid(ActiveDoors, 1), "Fast strip face retired its partner");
			const auto earned = ActiveDoors[0];
			char save[] = "strip-pair.sav", description[] = "Unequal strips";
			stop_time();
			require(state_save_all_sub(save, description), "Cannot save unequal strip pair");
			require(load_base_mission("d2", error, sizeof(error)), error);
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(state_restore_all_sub(save, 0) && xl_pog_door_frames(a) == 16 && xl_pog_door_frames(b) == 8 &&
			            xl_door_phases_snapshot() == phases && Num_open_doors == 1 &&
			            !std::memcmp(&earned, ActiveDoors, sizeof(earned)),
			        "Unequal strip partial completion did not survive stock");
			FrameTime = F1_0 / 64;
			saved = true;
		}
	}
	require(unequal && saved && Walls[0].state == WALL_DOOR_WAITING && Walls[1].state == WALL_DOOR_WAITING &&
	            Num_open_doors == 1 && !ActiveDoors[0].time,
	        "Unequal strip pair did not earn a fresh wait timer");
	std::cout << "Derived unequal strips: distinct counts/periods, collision thresholds, partial-completion cold restore and shared completion passed\n";
}

static void door_strip_lifecycle(const std::filesystem::path &root)
{
	write_door_strip_fixture(root);
	char error[256]{};
	require(load_base_mission("stripdr", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const int clip = Walls[0].clip_num, independent = Num_walls - 2;
	require(xl_pog_door_frames(clip) == 16 && independent > 1, "Loader omitted derived strip owner");
	for (int object = Highest_object_index; object >= 0; --object)
		if (Objects[object].type != OBJ_NONE && object != ConsoleObject - Objects) obj_delete(object);
	obj_relink(ConsoleObject - Objects, 10);
	compute_segment_center(&ConsoleObject->pos, &Segments[10]);
	ConsoleObject->size = F1_0 / 16;
	ConsoleObject->mtype.phys_info.velocity = {};
	for (int index : { 0, 1, independent, independent + 1 }) {
		auto &wall = Walls[index];
		auto &surface = Segments[wall.segnum].sides[wall.sidenum];
		surface.tmap_num = WallAnims[clip].frames[0];
		surface.tmap_num2 = 0;
	}
	const auto phase = [&](int index) {
		return xl_door_phases_snapshot()[Walls[index].segnum][Walls[index].sidenum].get<int>();
	};
	const auto pixels = [&](int index) {
		const auto &wall = Walls[index];
		const int texture = Segments[wall.segnum].sides[wall.sidenum].tmap_num;
		return xl_door_side_bitmap(wall.segnum, wall.sidenum, texture, &GameBitmaps[Textures[texture].index]);
	};
	const auto save_through_stock = [&] {
		const auto phases = xl_door_phases_snapshot();
		const std::vector<wall> walls(Walls, Walls + Num_walls);
		const std::vector<active_door> doors(ActiveDoors, ActiveDoors + Num_open_doors);
		char save[] = "strip-door.sav", description[] = "Derived strip lifecycle";
		stop_time();
		require(state_save_all_sub(save, description), "Strip door save failed");
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_door_phases_snapshot().empty() && !xl_pog_door_frames(clip), "Strip owner leaked into stock");
		require(state_restore_all_sub(save, 0) && xl_pog_door_frames(clip) == 16 && xl_door_phases_snapshot() == phases &&
		            std::size_t(Num_walls) == walls.size() && !std::memcmp(Walls, walls.data(), walls.size() * sizeof(wall)) &&
		            std::size_t(Num_open_doors) == doors.size() &&
		            (doors.empty() || !std::memcmp(ActiveDoors, doors.data(), doors.size() * sizeof(active_door))),
		        "Strip door cold restoration lost phases, walls or clocks");
		require(!game_is_time_paused(), "Strip save left simulation paused");
	};
	const int initial = phase(0);
	require((initial == -1 || initial == 1) && pixels(0)->bm_w == 8 && pixels(0)->bm_h == 8 && pixels(0)->bm_data[0] == 20, "Strip initial view differs");
	save_through_stock();
	wall_open_door(&Segments[Walls[0].segnum], Walls[0].sidenum);
	const fix step = WallAnims[clip].play_time / 16;
	FrameTime = step;
	for (int frame = 1; frame <= 9; ++frame) {
		wall_frame_process();
		require(phase(0) == -(frame + 1) && phase(1) == phase(0) &&
		            bool(Walls[0].flags & WALL_DOOR_OPENED) == (frame > 8),
		        "Strip source clock or collision threshold differs");
		require(xl_face_pixel_transparency(Walls[0].segnum, Walls[0].sidenum, 0, 0) == (frame & 1), "Strip collision pixels lost side phase");
		if (frame == 3) {
			save_through_stock();
			FrameTime = step;
			wall_open_door(&Segments[Walls[independent].segnum], Walls[independent].sidenum);
		}
		if (frame > 3) require(phase(independent) == -(frame - 2) && pixels(0) != pixels(independent), "Independent doors shared mutable strip phase");
	}
	save_through_stock();
	FrameTime = WallAnims[clip].play_time;
	wall_frame_process();
	require(Walls[0].state == WALL_DOOR_WAITING && phase(0) == -16, "Strip did not reach waiting at final frame");
	save_through_stock();
	// Forced close reverses the source waiting timer; earn a full period first
	FrameTime = WallAnims[clip].play_time;
	wall_frame_process();
	xl_authored_close_door(Walls[0].segnum, Walls[0].sidenum);
	FrameTime = step;
	wall_frame_process();
	require(Walls[0].state == WALL_DOOR_CLOSING && phase(0) == -15, "Strip closing clock differs");
	save_through_stock();
	wall_open_door(&Segments[Walls[0].segnum], Walls[0].sidenum);
	require(Walls[0].state == WALL_DOOR_OPENING, "Strip reversal failed");
	FrameTime = step;
	wall_frame_process();
	require(Walls[0].state == WALL_DOOR_WAITING && phase(0) == -16, "Strip reversal lost source remaining time");
	xl_authored_close_door(Walls[0].segnum, Walls[0].sidenum);
	FrameTime = WallAnims[clip].play_time;
	wall_frame_process();
	require(Walls[0].state == WALL_DOOR_CLOSED && phase(0) == -1, "Strip closure lost final source phase");
	save_through_stock();
	door_strip_restore_guards(root);
	const auto before_obstruction = xl_door_phases_snapshot();
	xl_door_obstruction_control::run(require);
	require(xl_door_phases_restore(before_obstruction, true), "Cannot restore derived obstruction fixture phases");
	std::cout << "Derived strip lifecycle: real loader, independent doors, opening thresholds, reversal and six disk-through-stock phases passed\n";
	std::cout << "Derived strips: nine malformed/source-identity extensions and 48 actor/effect obstruction cases passed\n";
	wall_open_door(&Segments[Walls[0].segnum], Walls[0].sidenum);
	FrameTime = 3 * step;
	wall_frame_process();
	wall_strip_recorded_future(root, "door");
}
