#include "../xl_vclip_frame.h"

static unsigned scoped_vclip_tick_control(int clip, int frames)
{
	struct Restore {
		object objects[2]{ Objects[0], Objects[1] };
		fix elapsed = FrameTime;
		~Restore()
		{
			Objects[0] = objects[0];
			Objects[1] = objects[1];
			FrameTime = elapsed;
		}
	} restore;
	unsigned ticks = 0;
	FrameTime = Vclip[clip].frame_time;
	for (int slot = 0; slot < 2; ++slot) {
		auto &actor = Objects[slot];
		actor = {};
		actor.type = OBJ_POWERUP;
		actor.id = POW_ENERGY;
		actor.render_type = RT_POWERUP;
		actor.lifeleft = IMMORTAL_TIME;
		actor.rtype.vclip_info.vclip_num = clip;
		int frame = 0, remaining = 0;
		for (int tick = 0; tick <= frames * 3; ++tick) {
			remaining -= FrameTime + ((FrameTime * slot) >> 4);
			while (remaining < 0) {
				remaining += Vclip[clip].frame_time;
				frame = (frame + (slot ? frames - 1 : 1)) % frames;
			}
			do_powerup_frame(&actor);
			require(actor.rtype.vclip_info.framenum == frame && actor.rtype.vclip_info.frametime == remaining,
			        "Native powerup tick lost replacement frame count, direction or period");
			++ticks;
		}
	}
	return ticks;
}

static nlohmann::json scoped_vclip_test(XlPreparedResources &prepared, const xl_assets::ObjectDependencies &graph)
{
	const auto &textures = *prepared.scoped_textures();
	const auto &playback = *prepared.scoped_texture_playback();
	require(!playback.clips.empty(), "Vclip control requires original authored strips");
	const auto sim_calls = d_rand_get_stream_call_count(D_RNG_SIM);
	uint32_t sim_state = 0, sim_after = 0;
	require(d_rand_get_stream_state(D_RNG_SIM, &sim_state), "Cannot inspect vclip SIM state");
	struct Retire {
		~Retire()
		{
			xl_pog_install(nullptr);
		}
	} retire;
	prepared.install_pog();
	nlohmann::json rows = nlohmann::json::array();
	unsigned comparisons = 0, native_ticks = 0;
	for (const auto &animation : playback.clips) {
		const auto &image = textures.images[animation.image].bitmap;
		const auto &clip = graph.clip_data[animation.clip];
		require(xl_pog_vclip_frames(animation.clip) == int(image.frames), "Native replacement vclip count differs");
		native_ticks += scoped_vclip_tick_control(animation.clip, int(image.frames));
		const auto bytes = image.pixels.size() / image.frames;
		const auto verify = [&](int frame) {
			const int bitmap = xl_pog_vclip_bitmap(animation.clip, frame);
			require(bitmap == animation.bitmaps.front(), "Native vclip lost its retained base bitmap");
			const auto &native = GameBitmaps[bitmap];
			require(native.bm_type == BM_RGBA && native.bm_h == int(image.frame_height()) &&
			            !std::memcmp(native.bm_data, image.pixels.data() + bytes * frame, bytes),
			        "Native object strip pixels differ");
			require(unsigned(native.bm_flags) == dxx_xl_texture_frame_flags(image.transparent_frames.data(), image.super_frames.data(), unsigned(frame)),
			        "Native object strip frame flags differ");
			xl_pog_pixel_view view{};
			require(xl_pog_physics_view(bitmap, &view) && view.height == image.height &&
			            !std::memcmp(view.pixels, image.pixels.data(), image.pixels.size()),
			        "Object frame selection changed retained physics pixels");
			++comparisons;
		};
		for (unsigned frame = 0; frame < image.frames; ++frame) {
			verify(int(frame));
			verify(int(image.frames - frame - 1));
			verify(int(frame));
		}
		require(xl_pog_vclip_bitmap(animation.clip, -1) == -1 && xl_pog_vclip_bitmap(animation.clip, int(image.frames)) == -1,
		        "Object strip accepted an out-of-range frame");
		for (int tick = 0; tick <= 120; ++tick) {
			const int remaining = int(int64_t(clip.play_time) * tick / 60);
			require(xl_pog_vclip_lifetime_frame(animation.clip, remaining) ==
			            dxx_xl_vclip_lifetime_frame(clip.play_time, clip.frame_time, int(image.frames), remaining),
			        "Native vclip lifetime selector differs from source-tested helper");
		}
		rows.push_back({ { "clip", animation.clip }, { "image", textures.images[animation.image].path }, { "native_frames", clip.num_frames }, { "replacement_frames", image.frames }, { "frame_time", clip.frame_time } });
	}
	// Equal-count candidates must match authored timing before retiring live storage
	auto changed = graph;
	std::vector<vclip> clips(graph.clip_data, graph.clip_data + graph.clip_count);
	++clips[playback.clips.front().clip].frame_time;
	changed.clip_data = clips.data();
	// This rejected candidate contains only scoped images, not the live owner's POG
	// Its bindings already exclude every POG-owned slot from original preparation
	auto scoped_only = textures;
	scoped_only.pog_indices.clear();
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> invalid(xl_pog_prepare_scoped(scoped_only, changed, nullptr), xl_pog_free);
	const int bitmap = playback.clips.front().bitmaps.front();
	const auto *pixels = GameBitmaps[bitmap].bm_data;
	require(xl_pog_prepare_storage(invalid.get()) && !xl_pog_install_prepared(invalid.get()) && GameBitmaps[bitmap].bm_data == pixels,
	        "Changed vclip definition replaced the live owner");
	prepared.install_pog();
	require(GameBitmaps[bitmap].bm_data == pixels, "Repeated vclip publication reset the frame");
	require(xl_pog_install(nullptr), "Vclip retirement failed");
	for (const auto &animation : playback.clips)
		require(!xl_pog_vclip_frames(animation.clip) && xl_pog_vclip_bitmap(animation.clip, 0) == -1, "Retirement retained vclip playback");
	// Exercise the signed native frame byte at the full supported strip boundary
	xl_compat::ScopedTextureResources boundary;
	boundary.images.push_back(textures.images[playback.clips.front().image]);
	auto &image = boundary.images.front().bitmap;
	image.width = 1;
	image.height = image.frames = 128;
	image.flags = 0;
	image.transparent_frames.fill(0);
	image.super_frames.fill(0);
	image.pixels.assign(128 * 4, 255);
	for (unsigned frame = 0; frame < 128; ++frame) image.pixels[frame * 4] = static_cast<unsigned char>(frame);
	const int boundary_clip = playback.clips.front().clip;
	for (int id : playback.clips.front().bitmaps) boundary.bindings.push_back({ id, 0 });
	auto boundary_graph = graph;
	boundary_graph.clips = { boundary_clip };
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> boundary_owner(xl_pog_prepare_scoped(boundary, boundary_graph, nullptr), xl_pog_free);
	require(xl_pog_prepare_storage(boundary_owner.get()) && xl_pog_install_prepared(boundary_owner.get()), "Cannot install bounded synthetic strip");
	const unsigned boundary_ticks = scoped_vclip_tick_control(boundary_clip, 128);
	require(xl_pog_install(nullptr), "Synthetic vclip retirement failed");
	require(d_rand_get_stream_state(D_RNG_SIM, &sim_after) && sim_after == sim_state &&
	            d_rand_get_stream_call_count(D_RNG_SIM) == sim_calls,
	        "Object strip preparation or playback changed SIM RNG");
	return { { "clips", rows }, { "pixel_comparisons", comparisons }, { "native_powerup_ticks", native_ticks }, { "synthetic_128_frame_ticks", boundary_ticks }, { "changed_definition_rejected", true }, { "sim_rng_unchanged", true }, { "repeat_and_retire", true }, { "limitation", "Native pixel views, controlled powerup ticks and lifetime selection; not rendered gameplay" } };
}
