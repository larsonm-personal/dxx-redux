#include "../particle_image.h"
#include "../scoped_texture.h"
#include "level_requirements.h"
#include "decoded_texture_test.h"
#include "scoped_texture_resources.h"
#include "scoped_texture_playback.h"
#include "scoped_texture_clock_test.h"
#include "scoped_vclip_test.h"
#include "scoped_texture_live_test.h"

// Inspect original inputs through their native owners without admitting gameplay
static int texture_dependency_audit(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		const char *name = find_arg_value(argc, argv, "-texture-dependency-audit");
		const char *level_text = find_arg_value(argc, argv, "-audit-level");
		const char *output = find_arg_value(argc, argv, "-audit-output");
		require(name && level_text && output, "Texture audit needs mission, level and output");
		const char *extra = find_arg_value(argc, argv, "-extra-dir");
		require(extra && std::filesystem::weakly_canonical(output) == std::filesystem::weakly_canonical(std::filesystem::path(extra) / "texture-dependency-audit.json"),
		        "Texture audit output must use its isolated fixture directory");
		require(mount_extra_dir(extra, error, sizeof(error)), error);
		if (find_arg_value(argc, argv, "-audit-live-recovery")) return scoped_texture_live_test(argc, argv, extra);
		const bool selected_mission = find_arg_value(argc, argv, "-audit-selected-mission") != nullptr;
		if (selected_mission) require(load_base_mission(name, error, sizeof(error)), error);
		char *end = nullptr;
		const long ordinal = std::strtol(level_text, &end, 10);
		require(end && !*end && ordinal > 0 && ordinal <= 1000, "Invalid texture audit level");
		const auto outgoing = gameplay_definitions();
		const auto live = Current_mission;
		auto *target = mission_prepare_by_name(name);
		require(target, "Cannot prepare texture audit descriptor");
		struct Release {
			mission_preparation **target;
			~Release()
			{
				mission_discard_prepared(target);
			}
		} release{ &target };
		xl_compat::TargetSources sources(target, "");
		const char *level_name = mission_prepared_level(target, int(ordinal));
		require(level_name, "Texture audit level is absent from descriptor");
		const auto bytes = sources.archive_bytes(level_name, 64 * 1024 * 1024);
		require(bool(bytes), "Texture audit level is absent from original HOG");
		const auto level = xl_compat::inspect_level(*bytes);
		auto prepared = XlPreparedResources::prepare_level_inputs(target, int(ordinal));
		const auto *retained_textures = prepared->scoped_textures();
		require(retained_textures, "Incoming level did not retain scoped texture inputs");
		XlPreparedResources moved(std::move(*prepared));
		require(!prepared->scoped_textures() && moved.scoped_textures() == retained_textures,
		        "Scoped textures did not follow resource move construction");
		*prepared = std::move(moved);
		require(!moved.scoped_textures() && prepared->scoped_textures() == retained_textures,
		        "Scoped textures did not follow resource move assignment");
		std::vector<std::string> retained_pixel_hashes;
		for (const auto &image : retained_textures->images)
			retained_pixel_hashes.push_back(picosha2::hash256_hex_string(image.bitmap.pixels));
		{
			auto independent = XlPreparedResources::prepare_level_inputs(target, int(ordinal));
			const auto *other = independent->scoped_textures();
			require(other && other != retained_textures && other->images.size() == retained_textures->images.size(),
			        "Independent scoped texture ownership differs");
			for (std::size_t i = 0; i < other->images.size(); ++i) {
				const auto &a = retained_textures->images[i], &b = other->images[i];
				require(a.hash == b.hash && a.bitmap.pixels == b.bitmap.pixels && a.bitmap.pixels.data() != b.bitmap.pixels.data(),
				        "Independent scoped textures share mutable pixel storage");
			}
		}
		unsigned late_preparation_controls = 0;
		const auto rejects_late_preparation = [&](const auto &operation) {
			try {
				operation();
			} catch (const std::exception &error) {
				require(std::string(error.what()).find("before scoped textures") != std::string::npos,
				        "Late resource preparation rejected for an unexpected reason");
				++late_preparation_controls;
				return;
			}
			throw std::runtime_error("Late resource preparation invalidated retained textures");
		};
		rejects_late_preparation([&] { prepared->prepare_banks(level.palette); });
		rejects_late_preparation([&] { prepared->prepare_hxm(nullptr); });
		rejects_late_preparation([&] { prepared->prepare_vham(std::vector<unsigned char>{}); });
		rejects_late_preparation([&] { prepared->prepare_pog(nullptr); });
		rejects_late_preparation([&] { prepared->prepare_camera_model(); });
		for (std::size_t i = 0; i < retained_textures->images.size(); ++i)
			require(picosha2::hash256_hex_string(retained_textures->images[i].bitmap.pixels) == retained_pixel_hashes[i],
			        "Independent discard or rejected late preparation changed retained pixels");
		const auto &graph = prepared->dependencies();
		const auto *bank = graph.resource_banks;
		require(bank && !bank->bitmap_name(0) && !bank->bitmap_name(bank->bitmap_count()), "Invalid retained bitmap name bounds");
		std::set<int> texture_ids;
		for (const auto &segment : level.segments)
			for (const auto &side : segment.sides) {
				if (side.texture >= 0) texture_ids.insert(side.texture);
				if (side.overlay > 0) texture_ids.insert(side.overlay & 0x3fff);
			}
		nlohmann::json report{
			{ "schema", "dxx-xl-texture-dependency-audit-v1" }, { "status", "inspection_only" }, { "playable", false }, { "mission", name }, { "level", ordinal }, { "level_file", level_name }, { "palette", level.palette }, { "level_sha256", picosha2::hash256_hex_string(*bytes) }, { "bitmap_bank_sha256", bank->bitmap_hash() }, { "side_textures", nlohmann::json::array() }, { "bitmap_candidates", nlohmann::json::array() }, { "images", nlohmann::json::object() }, { "limitations", "Direct side references and owned TGA candidates only; no admission, animation selection or full dependency qualification" }
		};
		for (int id : texture_ids) {
			require(id < graph.texture_count, "Original side references missing texture definition");
			const int bitmap = graph.texture_data[id].index;
			const char *bitmap_name = bank->bitmap_name(bitmap);
			require(bitmap_name, "Original side references missing bitmap name");
			report["side_textures"].push_back({ { "texture", id }, { "bitmap", bitmap }, { "name", bitmap_name } });
		}
		auto surface_dependencies = graph;
		for (int id : texture_ids) surface_dependencies.texture(id);
		for (const auto &wall : level.walls) surface_dependencies.wall_clip(wall.clip);
		report["surface_dependency_bitmaps"] = nlohmann::json::array();
		for (int bitmap : surface_dependencies.bitmaps)
			report["surface_dependency_bitmaps"].push_back({ { "bitmap", bitmap }, { "name", bank->bitmap_name(bitmap) } });
		report["surface_effects"] = surface_dependencies.effects;
		report["surface_wall_clips"] = surface_dependencies.wall_clips;
		auto all_dependencies = graph;
		try {
			xl_collect_incoming_dependencies(level, false, all_dependencies);
			report["dependency_walk"] = "complete";
		} catch (const std::exception &error) {
			report["dependency_walk"] = "rejected";
			report["dependency_error"] = error.what();
		}
		report["robot_definitions"] = nlohmann::json::array();
		for (std::size_t index = 0; index < level.objects.size(); ++index) {
			const auto &object = level.objects[index];
			if (object.type != OBJ_ROBOT || object.id < 0 || object.id >= graph.robot_count) continue;
			const auto &robot = graph.robot_data[object.id];
			std::vector<int> gun_submodels;
			for (int gun = 0; gun < robot.n_guns && gun < MAX_GUNS; ++gun) gun_submodels.push_back(robot.gun_submodels[gun]);
			report["robot_definitions"].push_back({ { "object", index }, { "robot", object.id }, { "model", robot.model_num }, { "model_submodels", robot.model_num >= 0 && robot.model_num < graph.model_count ? graph.model_data[robot.model_num].n_models : -1 }, { "gun_submodels", gun_submodels } });
		}
		report["model_sources"] = prepared->sources();
		report["robot_count"] = graph.robot_count;
		report["model_count"] = graph.model_count;
		if (report["dependency_walk"] == "complete") {
			const auto &scoped = *prepared->scoped_textures();
			require(prepared->scoped_texture_playback(), "Incoming owner lost its texture playback plan");
			const auto &playback = *prepared->scoped_texture_playback();
			report["prepared_effects"] = nlohmann::json::array();
			for (const auto &clock : playback.effects)
				report["prepared_effects"].push_back({ { "effect", clock.effect }, { "image", clock.image }, { "frame_time", clock.frame_time }, { "frames", scoped.images[clock.image].bitmap.frames }, { "bitmaps", clock.bitmaps } });
			unsigned playback_rejections = 0;
			if (!playback.effects.empty()) {
				const auto &clock = playback.effects.front();
				for (int control = 0; control < 7; ++control) {
					auto invalid = all_dependencies;
					auto images = scoped;
					std::vector<eclip> effects(graph.effect_data, graph.effect_data + graph.effect_count);
					std::vector<wclip> walls(graph.wall_clip_data, graph.wall_clip_data + graph.wall_clip_count);
					std::vector<vclip> clips(graph.clip_data, graph.clip_data + graph.clip_count);
					if (control == 0) effects[clock.effect].vc.frame_time = 0;
					if (control == 1) {
						effects.push_back(effects[clock.effect]);
						invalid.effect_count = int(effects.size());
					}
					if (control == 2) effects[clock.effect].changing_object_texture = 0;
					if (control == 3) {
						walls.at(0).num_frames = 1;
						walls[0].frames[0] = effects[clock.effect].changing_wall_texture;
						invalid.wall_clips.insert(0);
					}
					if (control == 4) {
						clips.at(0).num_frames = 1;
						clips[0].frames[0].index = clock.bitmaps.front();
						invalid.clips.insert(0);
					}
					if (control == 5) images.bindings.push_back(images.bindings.front());
					if (control == 6) images.pog_indices.insert(clock.bitmaps.front());
					invalid.effect_data = effects.data();
					invalid.wall_clip_data = walls.data();
					invalid.clip_data = clips.data();
					try {
						(void) xl_compat::ScopedTexturePlayback::prepare(images, invalid);
					} catch (const std::runtime_error &) {
						++playback_rejections;
					}
				}
				require(playback_rejections == 7, "Unsupported scoped playback plan was accepted");
			}
			report["playback_rejection_controls"] = playback_rejections;
			report["scoped_owner_controls"] = { { "move_construction", true }, { "move_assignment", true }, { "independent_discard", true }, { "late_preparation_rejections", late_preparation_controls } };
			report["selected_images"] = nlohmann::json::array();
			for (const auto &image : scoped.images)
				report["selected_images"].push_back({ { "path", image.path }, { "sha256", image.hash }, { "frames", image.bitmap.frames }, { "width", image.bitmap.width }, { "height", image.bitmap.height } });
			report["selected_bindings"] = nlohmann::json::array();
			for (const auto &binding : scoped.bindings)
				report["selected_bindings"].push_back({ { "bitmap", binding.bitmap }, { "image", binding.image } });
			if (!scoped.bindings.empty()) {
				const auto &binding = scoped.bindings.front();
				auto image = scoped.images[binding.image].bitmap;
				image.index = binding.bitmap;
				std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> mask(xl_pog_prepare_rgba({ image }, bank->bitmap_count()), xl_pog_free);
				const auto masked = xl_compat::ScopedTextureResources::prepare(sources, all_dependencies, int(ordinal), mask.get());
				require(masked.bindings.size() + 1 == scoped.bindings.size() &&
				            std::none_of(masked.bindings.begin(), masked.bindings.end(), [&](const auto &row) { return row.bitmap == binding.bitmap; }),
				        "Prepared POG target did not take precedence over scoped TGA");
				report["pog_precedence_control"] = binding.bitmap;
			}
			report["effect_clocks"] = nlohmann::json::array();
			for (int id : all_dependencies.effects) {
				const auto &effect = graph.effect_data[id];
				std::vector<int> frames;
				for (int frame = 0; frame < effect.vc.num_frames; ++frame) frames.push_back(effect.vc.frames[frame].index);
				report["effect_clocks"].push_back({ { "effect", id }, { "frame_time", effect.vc.frame_time }, { "wall_texture", effect.changing_wall_texture }, { "object_texture", effect.changing_object_texture }, { "bitmaps", frames } });
			}
			report["wall_clocks"] = nlohmann::json::array();
			for (int id : all_dependencies.wall_clips) {
				const auto &clip = graph.wall_clip_data[id];
				std::vector<int> frames;
				for (int frame = 0; frame < clip.num_frames; ++frame) frames.push_back(graph.texture_data[clip.frames[frame]].index);
				report["wall_clocks"].push_back({ { "clip", id }, { "play_time", clip.play_time }, { "flags", clip.flags }, { "bitmaps", frames } });
			}
		}
		// Exercise the actual retained authored gun links and rejection bounds
		unsigned gun_controls = 0;
		for (const auto &object : level.objects) {
			if (object.type != OBJ_ROBOT || object.id < 0 || object.id >= graph.robot_count) continue;
			const auto &robot = graph.robot_data[object.id];
			if (!robot.n_guns || robot.model_num < 0 || robot.model_num >= graph.model_count) continue;
			const int part = robot.gun_submodels[0];
			if (part < graph.model_data[robot.model_num].n_models || part >= MAX_SUBMODELS) continue;
			auto control = graph;
			control.robot(object.id);
			std::vector<polymodel> models(graph.model_data, graph.model_data + graph.model_count);
			for (int invalid_parent : { part, MAX_SUBMODELS }) {
				models[robot.model_num].submodel_parents[part] = invalid_parent;
				auto invalid = graph;
				invalid.model_data = models.data();
				bool rejected = false;
				try {
					invalid.robot(object.id);
				} catch (const std::exception &) {
					rejected = true;
				}
				require(rejected, "Invalid authored gun chain was accepted");
				++gun_controls;
			}
		}
		report["gun_chain_rejection_controls"] = gun_controls;
		report["prepared_level_inputs"] = true;
		report["dependency_bitmaps"] = nlohmann::json::array();
		for (int bitmap : all_dependencies.bitmaps)
			report["dependency_bitmaps"].push_back({ { "bitmap", bitmap }, { "name", bank->bitmap_name(bitmap) } });
		for (int bitmap = 1; bitmap < bank->bitmap_count(); ++bitmap) {
			const std::string bitmap_name = bank->bitmap_name(bitmap);
			const auto marker = bitmap_name.find('#');
			std::vector<std::string> paths;
			if (marker == std::string::npos)
				paths.push_back(xl_compat::static_mission_texture_path(sources.archive().c_str(), bitmap_name.c_str()));
			else {
				paths.push_back(xl_compat::animation_mission_texture_path(sources.archive().c_str(), bitmap_name.c_str()));
				if (bitmap_name.substr(marker) != "#0")
					paths.push_back(xl_compat::animation_mission_texture_path(sources.archive().c_str(), (bitmap_name.substr(0, marker) + "#0").c_str()));
			}
			for (const auto &path : paths) {
				if (path.empty()) continue;
				if (!report["images"].contains(path)) {
					const auto image_bytes = sources.directory_bytes(path, xl_compat::pog_file_limit + 65536);
					if (!image_bytes) continue;
					report["images"][path] = { { "sha256", picosha2::hash256_hex_string(*image_bytes) }, { "size", image_bytes->size() } };
					xl_compat::PogBitmap image;
					try {
						image = xl_compat::read_particle_image(image_bytes->data(), image_bytes->size());
					} catch (const std::exception &error) {
						report["images"][path]["decode_error"] = error.what();
						report["bitmap_candidates"].push_back({ { "bitmap", bitmap }, { "name", bitmap_name }, { "path", path } });
						continue;
					}
					unsigned clear = 0, soft = 0, super = 0;
					for (std::size_t pixel = 0; pixel < image.pixels.size(); pixel += 4) {
						const auto *rgba = image.pixels.data() + pixel;
						clear += rgba[3] == 0;
						soft += rgba[3] > 1 && rgba[3] < 255;
						super += rgba[0] == 120 && rgba[1] == 88 && rgba[2] == 128;
					}
					report["images"][path] = {
						{ "sha256", picosha2::hash256_hex_string(*image_bytes) },
						{ "pixels_sha256", picosha2::hash256_hex_string(image.pixels) },
						{ "source_bpp", image_bytes->at(16) },
						{ "width", image.width },
						{ "height", image.height },
						{ "frames", image.frames },
						{ "flags", image.flags },
						{ "clear_pixels", clear },
						{ "soft_pixels", soft },
						{ "key_color_pixels", super }
					};
				}
				report["bitmap_candidates"].push_back({ { "bitmap", bitmap }, { "name", bitmap_name }, { "path", path } });
			}
		}
		if (find_arg_value(argc, argv, "-audit-native-pixels")) report["native_pixel_controls"] = decoded_texture_owner_test(sources, graph);
		if (find_arg_value(argc, argv, "-audit-native-clock")) report["native_clock_controls"] = scoped_texture_clock_test(*prepared, all_dependencies);
		if (find_arg_value(argc, argv, "-audit-native-vclip")) report["native_vclip_controls"] = scoped_vclip_test(*prepared, all_dependencies);
		if (selected_mission) {
			XlPreparedResources selected(level_name, &level, int(ordinal));
			const auto *textures = selected.scoped_textures();
			require(textures && textures->images.size() == retained_textures->images.size() && textures->bindings.size() == retained_textures->bindings.size(),
			        "Selected mission and incoming texture ownership differ");
			for (std::size_t i = 0; i < textures->images.size(); ++i)
				require(textures->images[i].path == retained_textures->images[i].path && textures->images[i].hash == retained_textures->images[i].hash &&
				            textures->images[i].bitmap.pixels == retained_textures->images[i].bitmap.pixels,
				        "Selected mission used different authored texture bytes");
			auto selected_dependencies = selected.dependencies();
			selected_dependencies.require_owned_banks = true;
			xl_collect_incoming_dependencies(level, false, selected_dependencies);
			if (find_arg_value(argc, argv, "-audit-native-clock")) report["selected_native_clock_controls"] = scoped_texture_clock_test(selected, selected_dependencies);
			if (find_arg_value(argc, argv, "-audit-native-vclip")) report["selected_native_vclip_controls"] = scoped_vclip_test(selected, selected_dependencies);
		}
		prepared.reset();
		require(mission_discard_prepared(&target), "Texture audit descriptor did not release");
		require(Current_mission == live && gameplay_definitions() == outgoing, "Texture audit changed outgoing definitions");
		native_continuation_receipt(output, report);
		std::cout << "Original texture dependency audit: " << report["side_textures"].size() << " side textures, "
		          << report["images"].size() << " owned images; no gameplay admission\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Texture dependency audit failed: " << error.what() << '\n';
		return 1;
	}
}
