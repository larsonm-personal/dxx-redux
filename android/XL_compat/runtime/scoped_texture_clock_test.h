#include "scoped_texture_playback.h"
#include "../xl_texture_frame.h"

static nlohmann::json scoped_texture_clock_test(XlPreparedResources &prepared,
                                                const xl_assets::ObjectDependencies &graph)
{
	const auto *textures = prepared.scoped_textures();
	const auto *playback = prepared.scoped_texture_playback();
	require(textures && playback && playback->effects.size() == 1, "Clock control requires the original single replacement effect");
	const auto &clock = playback->effects.front();
	const auto &image = textures->images[clock.image].bitmap;
	require(clock.effect == 78 && image.frames == 6 && clock.frame_time == 6144, "Original force-field clock changed");
	auto &effect = Effects[clock.effect];
	const auto original_frame = effect.frame_count, original_time = effect.time_left, original_flags = effect.flags;
	const auto original_mapping = Textures[effect.changing_wall_texture];
	struct Restore {
		eclip &effect;
		int frame, time, flags;
		bitmap_index mapping;
		~Restore()
		{
			xl_pog_install(nullptr);
			effect.frame_count = frame;
			effect.time_left = time;
			effect.flags = flags;
			Textures[effect.changing_wall_texture] = mapping;
		}
	} restore{ effect, original_frame, original_time, original_flags, original_mapping };
	// Reject a differently authored candidate without swapping any live definitions
	auto changed = graph;
	std::vector<eclip> definitions(graph.effect_data, graph.effect_data + graph.effect_count);
	++definitions[clock.effect].vc.frame_time;
	changed.effect_data = definitions.data();
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> invalid(
	    xl_pog_prepare_scoped(*textures, changed, nullptr), xl_pog_free);
	require(xl_pog_prepare_storage(invalid.get()) && !xl_pog_install_prepared(invalid.get()),
	        "Scoped native publication accepted changed effect definitions");
	require(Num_effects == graph.effect_count, "Clock publication effect table counts differ");
	unsigned inactive_effects = 0;
	for (int id = 0; id < graph.effect_count; ++id) {
		const auto &incoming = graph.effect_data[id], &live = Effects[id];
		if (incoming.vc.num_frames == -1 && incoming.changing_wall_texture == -1 && incoming.changing_object_texture == -1) ++inactive_effects;
		if (incoming.vc.num_frames != live.vc.num_frames || incoming.vc.frame_time != live.vc.frame_time ||
		    incoming.changing_wall_texture != live.changing_wall_texture || incoming.changing_object_texture != live.changing_object_texture || incoming.crit_clip != live.crit_clip)
			throw std::runtime_error("Clock publication differs from live effect " + std::to_string(id));
	}
	prepared.install_pog();
	require(xl_pog_effect_frames(&effect) == 6 && effect.vc.num_frames == 8 && effect.vc.frame_time == 6144,
	        "Scoped clock rewrote authored definitions or lost its replacement count");
	const auto frame_bytes = image.pixels.size() / image.frames;
	unsigned comparisons = 0;
	for (int tick = 0; tick <= 180; ++tick) {
		const fix64 elapsed = fix64(tick) * F1_0 / 60;
		effect_get_loop_state(&effect, elapsed, &effect.frame_count, &effect.time_left);
		const int expected = elapsed <= 0 ? 0 : int(((elapsed - 1) / clock.frame_time) % image.frames);
		require(effect.frame_count == expected, "Scoped reconstructed phase differs from its authored period and replacement count");
		effect_apply_bitmap_state(clock.effect);
		for (int bitmap : clock.bitmaps) {
			const auto &native = GameBitmaps[bitmap];
			require(native.bm_data == GameBitmaps[clock.bitmaps.front()].bm_data,
			        "Scoped aliases duplicated their retained strip storage");
			require(native.bm_type == BM_RGBA && native.bm_h == int(image.frame_height()) &&
			            !std::memcmp(native.bm_data, image.pixels.data() + frame_bytes * expected, frame_bytes),
			        "Scoped native display alias selected different frame pixels");
			require(unsigned(native.bm_flags) == dxx_xl_texture_frame_flags(image.transparent_frames.data(), image.super_frames.data(), unsigned(expected)),
			        "Scoped native display alias has the wrong per-frame transparency flags");
			xl_pog_pixel_view view{};
			require(xl_pog_physics_view(bitmap, &view) && view.height == image.height &&
			            !std::memcmp(view.pixels, image.pixels.data(), image.pixels.size()),
			        "Scoped native clock changed its whole-strip physics view");
			++comparisons;
		}
		if (tick == 75) {
			const auto *pixels = GameBitmaps[clock.bitmaps.front()].bm_data;
			prepared.install_pog();
			require(GameBitmaps[clock.bitmaps.front()].bm_data == pixels && effect.frame_count == expected,
			        "Repeated scoped publication reset an active display phase");
		}
	}
	stop_effect(clock.effect);
	require(effect.frame_count == 0 && (effect.flags & EF_STOPPED) &&
	            !std::memcmp(GameBitmaps[clock.bitmaps.front()].bm_data, image.pixels.data(), frame_bytes),
	        "Stopping a scoped effect did not select its original first frame");
	require(xl_pog_install(nullptr) && xl_pog_effect_frames(&effect) == effect.vc.num_frames,
	        "Scoped effect retirement retained its replacement clock");
	for (int bitmap : clock.bitmaps) {
		xl_pog_pixel_view view{};
		require(!xl_pog_physics_view(bitmap, &view), "Scoped retirement retained a borrowed physics view");
	}
	return { { "effect", clock.effect }, { "frame_count", image.frames }, { "frame_time", clock.frame_time }, { "unchanged_inactive_effects", inactive_effects }, { "ticks", 181 }, { "alias_pixel_comparisons", comparisons }, { "changed_definition_rejection", true }, { "repeat_preserved_phase", true }, { "stop_and_retire", true }, { "limitation", "Native phase reconstruction and mapping; no rendered frame or gameplay tick qualification" } };
}
