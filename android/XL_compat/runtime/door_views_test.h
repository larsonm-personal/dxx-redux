#include "scoped_texture_playback.h"
#include "door_phases_test.h"

static void descriptor_door_view_controls()
{
	XlPreparedResources retained("d2leva-1.rl2");
	retained.prepare_banks("groupa.256");
	auto graph = retained.dependencies();
	graph.require_owned_banks = true;
	graph.wall(WALL_DOOR, 0);
	const auto &clip = graph.wall_clip_data[0];
	xl_compat::ScopedTextureResources images;
	xl_compat::PogBitmap image;
	image.width = 4;
	image.height = 64;
	image.frames = 16;
	image.truecolor = true;
	image.pixels.resize(4 * image.width * image.height);
	for (unsigned frame = 0; frame < image.frames; ++frame)
		for (unsigned pixel = 0; pixel < 16; ++pixel) {
			const auto at = (frame * 16 + pixel) * 4;
			image.pixels[at] = static_cast<unsigned char>(frame + 10);
			image.pixels[at + 1] = 30;
			image.pixels[at + 2] = 50;
			image.pixels[at + 3] = frame & 1 ? 0 : 255;
		}
	xl_compat::prepare_pog_rgba(image);
	images.images.push_back({ "derived-door-control", "derived", image });
	std::set<int> aliases;
	for (int frame = 0; frame < clip.num_frames; ++frame) aliases.insert(graph.texture_data[clip.frames[frame]].index);
	for (int bitmap : aliases) images.bindings.push_back({ bitmap, 0 });
	const auto plan = xl_compat::ScopedTexturePlayback::prepare(images, graph);
	require(plan.doors.size() == 1 && plan.doors[0].clip == 0, "Derived door binding was not retained");
	const auto definitions = gameplay_definitions();
	using Owner = std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)>;
	const auto prepare = [&](const auto &inputs, const auto &tables) {
		Owner owner(xl_pog_prepare_scoped(inputs, tables, nullptr), xl_pog_free);
		require(xl_pog_prepare_storage(owner.get()), "Derived door frame storage allocation failed");
		return owner;
	};
	struct Restore {
		~Restore()
		{
			xl_door_phases_clear();
			xl_pog_install(nullptr);
		}
	} restore;
	auto first = prepare(images, graph);
	require(!xl_pog_side_frame(*aliases.begin(), 0), "Preparation published side frames");
	require(xl_pog_install_prepared(first.get()) && xl_pog_door_frames(0) == 16, "Door replacement clock was not installed");
	descriptor_door_phase_controls(graph, plan);
	const auto *first_view = xl_pog_side_frame(*aliases.begin(), 0);
	require(first_view && first_view->bm_data[0] == 10, "Door frame zero pixels differ");
	const auto *native_pixels = GameBitmaps[*aliases.begin()].bm_data;
	for (unsigned frame = 0; frame < 16; ++frame) {
		for (int bitmap : aliases) {
			const auto *view = xl_pog_side_frame(bitmap, int(frame));
			require(view && view == xl_pog_side_frame(*aliases.begin(), int(frame)) && view->bm_w == 4 && view->bm_h == 4 &&
			            view->bm_data[0] == (frame & 1 ? 0 : frame + 10) && view->bm_flags == (frame ? BM_FLAG_TRANSPARENT : 0),
			        "Door aliases lost immutable frame pixels or transparency");
		}
		require(first_view->bm_data[0] == 10 && GameBitmaps[*aliases.begin()].bm_data == native_pixels,
		        "Independent side selection changed another phase or shared native bitmap");
	}
	require(!xl_pog_side_frame(*aliases.begin(), -1) && !xl_pog_side_frame(*aliases.begin(), 16) &&
	            !xl_pog_side_frame(-1, 0) && !xl_pog_side_frame(0, 0),
	        "Door frame lookup accepted invalid bounds");
	for (int attempt = 0; attempt < 3; ++attempt) {
		auto discarded = prepare(images, graph);
		require(xl_pog_install_prepared(first.get()) && xl_pog_side_frame(*aliases.begin(), 0) == first_view,
		        "Repeated publication or independent preparation changed live side views");
	}
	first.reset();
	require(xl_pog_side_frame(*aliases.begin(), 0) == first_view && first_view->bm_data[0] == 10,
	        "Prepared wrapper destruction retired transferred native views");
	std::vector<wclip> changed_walls(graph.wall_clip_data, graph.wall_clip_data + graph.wall_clip_count);
	++changed_walls[0].play_time;
	auto changed_graph = graph;
	changed_graph.wall_clip_data = changed_walls.data();
	auto stale = prepare(images, changed_graph);
	require(!xl_pog_install_prepared(stale.get()) && xl_pog_side_frame(*aliases.begin(), 0) == first_view,
	        "Changed retained wall definition replaced live frame owner");
	unsigned rejections = 0;
	for (int control = 0; control < 9; ++control) {
		auto invalid = graph;
		auto inputs = images;
		std::vector<eclip> effects(graph.effect_data, graph.effect_data + graph.effect_count);
		std::vector<vclip> clips(graph.clip_data, graph.clip_data + graph.clip_count);
		if (control == 0) invalid.other_wall_clips.insert(0);
		if (control == 1) invalid.door_clips.clear();
		if (control == 2) inputs.pog_indices.insert(*aliases.begin());
		if (control == 3) inputs.bindings.push_back(inputs.bindings.front());
		if (control == 4) {
			inputs.images.push_back(inputs.images.front());
			inputs.bindings.back().image = 1;
		}
		if (control == 5) {
			changed_walls[0].play_time = 1;
			invalid.wall_clip_data = changed_walls.data();
		}
		if (control == 6) {
			effects[0].changing_wall_texture = clip.frames[0];
			effects[0].changing_object_texture = -1;
			effects[0].vc.num_frames = 1;
			effects[0].vc.frames[0].index = graph.texture_data[clip.frames[0]].index;
			effects[0].vc.frame_time = 1;
			invalid.effect_data = effects.data();
		}
		if (control == 7) {
			clips[0].num_frames = 1;
			clips[0].frames[0].index = graph.texture_data[clip.frames[0]].index;
			invalid.clip_data = clips.data();
			invalid.clips.insert(0);
		}
		if (control == 8)
			inputs.bindings.erase(std::remove_if(inputs.bindings.begin(), inputs.bindings.end(), [&](const auto &binding) {
				                      return binding.bitmap == graph.texture_data[clip.frames[0]].index;
			                      }),
			                      inputs.bindings.end());
		try {
			xl_compat::ScopedTexturePlayback::prepare(inputs, invalid);
		} catch (const std::runtime_error &) {
			++rejections;
		}
	}
	require(rejections == 9 && xl_pog_side_frame(*aliases.begin(), 0) == first_view,
	        "Malformed or conflicting door preparation altered the live owner");
	xl_pog_side_views_discard_context();
	require(xl_pog_side_frame(*aliases.begin(), 0) == first_view && first_view->bm_data[0] == 10,
	        "Context retirement lost retained frame storage");
	for (std::size_t pixel = 0; pixel < images.images[0].bitmap.pixels.size(); pixel += 4)
		images.images[0].bitmap.pixels[pixel] = 77;
	xl_compat::prepare_pog_rgba(images.images[0].bitmap);
	auto second = prepare(images, graph);
	require(first_view->bm_data[0] == 10 && xl_pog_install_prepared(second.get()) &&
	            xl_pog_side_frame(*aliases.begin(), 0)->bm_data[0] == 77,
	        "Equal-count door owner reused prior pixels");
	require(xl_pog_install(nullptr) && !xl_pog_door_frames(0) && !xl_pog_side_frame(*aliases.begin(), 0) &&
	            gameplay_definitions() == definitions,
	        "Door native retirement leaked a clock, view or definition");
	std::cout << "Derived door views: retained definitions, 16 immutable phases, shared aliases, independent discard, ownership transfer, nine malformed controls and retirement passed\n";

	image.index = graph.texture_data[clip.frames[0]].index;
	Owner packed(xl_pog_prepare_rgba({ image }, graph.resource_banks->bitmap_count()), xl_pog_free);
	xl_compat::ScopedTexturePlayback packed_plan;
	packed_plan.doors = xl_pog_bind_animation(*packed, graph, graph.resource_banks->bitmap_count());
	require(packed_plan.doors.size() == 1 && xl_pog_count(packed.get()) == int(aliases.size()) &&
	            !xl_pog_side_frame(image.index, 0),
	        "Packed wall preparation omitted aliases or published them prematurely");
	require(xl_pog_install(packed.get()) && xl_pog_door_frames(0) == 16 && !xl_pog_scoped_owner(packed.get()),
	        "Packed wall strip failed to install without scoped textures");
	for (int bitmap : aliases)
		require(xl_pog_side_frame(bitmap, 15) == xl_pog_side_frame(image.index, 15),
		        "Packed wall aliases do not share their immutable views");
	descriptor_door_phase_controls(graph, packed_plan);
	require(xl_pog_install(nullptr) && !xl_pog_door_frames(0) && !xl_pog_side_frame(image.index, 0) &&
	            gameplay_definitions() == definitions,
	        "Packed wall strip survived retirement");
	auto dormant_graph = graph;
	dormant_graph.wall_clips.clear();
	dormant_graph.door_clips.clear();
	dormant_graph.blastable_clips.clear();
	dormant_graph.other_wall_clips.clear();
	Owner dormant(xl_pog_prepare_rgba({ image }, dormant_graph.resource_banks->bitmap_count()), xl_pog_free);
	const auto dormant_plan = xl_pog_bind_animation(*dormant, dormant_graph, dormant_graph.resource_banks->bitmap_count());
	require(dormant_plan.size() == 1 && dormant_plan[0].clip == 0 && xl_pog_count(dormant.get()) == int(aliases.size()) &&
	            xl_pog_install(dormant.get()) && xl_pog_door_frames(0) == 16,
	        "Unused original wall clock did not retain its qualified first-frame POG strip");
	for (int bitmap : aliases)
		require(xl_pog_side_frame(bitmap, 15) == xl_pog_side_frame(image.index, 15),
		        "Unused original wall clock lost a POG frame alias");
	require(xl_pog_install(nullptr) && !xl_pog_door_frames(0) && !xl_pog_side_frame(image.index, 0),
	        "Unused original wall clock survived retirement");
	auto unsupported_graph = dormant_graph;
	unsupported_graph.wall_clips.insert(0);
	unsupported_graph.other_wall_clips.insert(0);
	Owner unsupported(xl_pog_prepare_rgba({ image }, unsupported_graph.resource_banks->bitmap_count()), xl_pog_free);
	bool rejected_other_wall = false;
	try {
		xl_pog_bind_animation(*unsupported, unsupported_graph, unsupported_graph.resource_banks->bitmap_count());
	} catch (const std::runtime_error &) {
		rejected_other_wall = true;
	}
	require(rejected_other_wall, "Active unsupported wall use was mistaken for an unused original clock");
	std::cout << "Packed wall strip: alias installation, independent pixel and collision phases, restore and retirement passed\n";
}
