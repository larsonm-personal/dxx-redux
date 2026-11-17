// Explicit monitor visibility is a rendering assist, not an earned traversal
template <class Sample>
nlohmann::json camera_wall_monitor_probe(Sample sample)
{
	const auto require = [](bool valid, const char *message) { if (!valid) throw std::runtime_error(message); };
	require(game_is_time_paused() && gl_initialized && xl_level_extensions_active(), "Wall monitor proof requires a paused XL level and GL context");
	unsigned walls = 0;
	for (int i = 0; i < Num_walls; ++i) {
		const int trigger = wall_trigger_index(Walls[i].trigger);
		walls += trigger >= 0 && trigger < Num_triggers && Triggers[trigger].type == 16;
	}
	require(walls > 0, "Wall monitor proof contains no wall camera sources");
	const auto sim_calls = d_rand_get_call_count();
	unsigned before, after;
	require(d_rand_get_state(&before), "Wall monitor SIM state unavailable");
	object *const viewer = Viewer;
	const int object_count = Highest_object_index;
	nlohmann::json captures = nlohmann::json::array();
	for (int segment = 0; segment < Num_segments; ++segment)
		for (int side = 0; side < 6; ++side) {
			if (!xl_camera_monitor_face(segment, side)) continue;
			int vertices[4];
			get_side_verts(vertices, segment, side);
			const int overlay = Segments[segment].sides[side].tmap_num2;
			grs_bitmap original{}, *image = nullptr;
			for (int attempt = 0; attempt < 102; ++attempt) {
				xl_camera_renderer_frame();
				xl_camera_monitor_begin(segment, side, 4, vertices, overlay);
				image = xl_camera_monitor_bitmap(0, &original);
				if (image == &original && overlay) image = xl_camera_monitor_bitmap(1, &original);
				xl_camera_monitor_end();
				if (image != &original) break;
			}
			require(image && image != &original && image->gltexture, "Wall monitor never captured a native scene");
			const auto pixels = sample(image->gltexture->handle, image->bm_w, image->bm_h);
			unsigned visible = 0;
			for (std::size_t i = 0; i < pixels.size(); i += 4) {
				require(pixels[i + 3] == 255, "Wall monitor lost opacity");
				visible += pixels[i] > 8 || pixels[i + 1] > 8 || pixels[i + 2] > 8;
			}
			require(visible > pixels.size() / 400, "Wall monitor lacks visible scene information");
			captures.push_back({ { "segment", segment }, { "side", side }, { "width", image->bm_w }, { "height", image->bm_h }, { "visible_pixels", visible } });
		}
	require(!captures.empty() && glGetError() == GL_NO_ERROR, "Wall monitor proof lacks clean captures");
	require(Viewer == viewer && Highest_object_index == object_count && d_rand_get_state(&after) && before == after && sim_calls == d_rand_get_call_count(), "Wall camera capture changed viewer ownership, object allocation or SIM RNG");
	return nlohmann::json{ { "passed", bool{ true } }, { "assisted_visibility", bool{ true } }, { "wall_sources", walls }, { "captures", captures } };
}
