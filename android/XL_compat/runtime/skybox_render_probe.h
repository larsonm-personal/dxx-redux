// Android loaded-level skybox controls, using the ordinary frame and MSAA resolve
std::vector<unsigned char> skybox_probe_pixels()
{
	render_frame(0, 0);
	ogl_prepare_framebuffer_readback();
	GLint viewport[4], read, pack, alignment, row, skip_rows, skip_pixels;
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	require(viewport[2] >= 64 && viewport[3] >= 64, "Skybox scene viewport is too small");
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	std::vector<unsigned char> result(32 * 32 * 4);
	glReadPixels(viewport[0] + viewport[2] / 2 - 16, viewport[1] + viewport[3] / 2 - 16, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
	const auto error = glGetError();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	require(error == GL_NO_ERROR && glGetError() == GL_NO_ERROR, "Skybox scene readback failed");
	return result;
}

void skybox_render_depth_controls()
{
	Readback readback;
	ParticleProbeTarget target;
	struct Depth {
		GLuint buffer = 0;
		GLboolean write, test, blend, cull;
		GLint function;
		GLfloat clear;
		int texture = GL_TEXTURE_2D_enabled;
		Depth()
		{
			glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
			glGetIntegerv(GL_DEPTH_FUNC, &function);
			glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear);
			test = glIsEnabled(GL_DEPTH_TEST);
			blend = glIsEnabled(GL_BLEND);
			cull = glIsEnabled(GL_CULL_FACE);
			glGenRenderbuffers(1, &buffer);
			glBindRenderbuffer(GL_RENDERBUFFER, buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 16, 16);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffer);
			glDisable(GL_CULL_FACE);
		}
		~Depth()
		{
			glDeleteRenderbuffers(1, &buffer);
			// Keep the native enable cache consistent with the shim after the witness
			if (texture == 1) glEnable(GL_TEXTURE_2D);
			else glDisable(GL_TEXTURE_2D);
			GL_TEXTURE_2D_enabled = texture;
			glDepthMask(write);
			glDepthFunc(function);
			glClearDepthf(clear);
			if (test) glEnable(GL_DEPTH_TEST);
			else glDisable(GL_DEPTH_TEST);
			if (blend) glEnable(GL_BLEND);
			else glDisable(GL_BLEND);
			if (cull) glEnable(GL_CULL_FACE);
			else glDisable(GL_CULL_FACE);
		}
	} depth;
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Skybox depth target is incomplete");
	const auto sample = [] {
		std::array<unsigned char, 4> pixel{};
		glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
		require(glGetError() == GL_NO_ERROR, "Skybox depth control raised a GPU error");
		return pixel;
	};
	const auto clear = [](float value) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glFrustumf(-1, 1, -1, 1, 1, 5000);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDepthMask(GL_TRUE);
		glClearDepthf(value);
		glClearColor(1, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// The pass must temporarily establish its own depth state and restore this
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_GREATER);
		glDisable(GL_DEPTH_TEST);
	};
	const auto render = [] {
		xl_skybox_render_scene();
		GLboolean write;
		GLint function;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
		glGetIntegerv(GL_DEPTH_FUNC, &function);
		require(!write && !glIsEnabled(GL_DEPTH_TEST) && function == GL_GREATER && !xl_skybox_rendering(), "Skybox pass leaked depth or full-bright state");
	};
	clear(1);
	render();
	require(sample() == std::array<unsigned char, 4>{ 0, 255, 0, 255 }, "Skybox does not draw with open depth");
	const auto far_quad = [] {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glActiveTexture(GL_TEXTURE1);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		gles3_shim_use_external(0);
		glColor4f(0, 0, 1, 1);
		const GLfloat vertices[]{ -1, -1, .999f, 1, -1, .999f, -1, 1, .999f, 1, 1, .999f };
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vertices);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableClientState(GL_VERTEX_ARRAY);
	};
	// Only replace color; a later distant primitive must see the skybox's depth
	glClearColor(1, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	far_quad();
	require(sample() == std::array<unsigned char, 4>{ 255, 0, 0, 255 }, "Skybox did not write depth for later rendering");
	clear(1);
	far_quad();
	require(sample() == std::array<unsigned char, 4>{ 0, 0, 255, 255 }, "Skybox depth witness does not draw without an occluder");
	clear(0);
	render();
	require(sample() == std::array<unsigned char, 4>{ 255, 0, 0, 255 }, "Skybox ignored foreground depth");
	con_printf(CON_NORMAL, "XL skybox depth: actual loaded-face draw, foreground rejection, later primitive occlusion and depth-state restoration passed\n");
}

void skybox_render_gpu_controls(const xl_compat::Inventory &original)
{
	require(gl_initialized && game_is_time_paused(), "Skybox scene requires paused headed gameplay");
	int selected_segment = -1, selected_side = -1;
	for (int segment = 0; segment <= Highest_segment_index && selected_segment < 0; ++segment) {
		if (Segments[segment].objects >= 0) continue;
		for (int side = 0; side < 6; ++side)
			if (Segments[segment].children[side] == -1 && Segments[segment].sides[side].wall_num < 0) {
				selected_segment = segment;
				selected_side = side;
				break;
			}
	}
	require(selected_segment >= 0, "Skybox scene requires an empty segment with a solid side");
	auto &surface = Segments[selected_segment].sides[selected_side];
	auto &bitmap = GameBitmaps[Textures[surface.tmap_num].index];
	struct Restore {
		const xl_compat::Inventory &original;
		side &surface;
		grs_bitmap &bitmap;
		side saved_surface;
		grs_bitmap saved_bitmap;
		vms_vector position = ConsoleObject->pos;
		vms_matrix orientation = ConsoleObject->orient;
		int segment = ConsoleObject->segnum;
		object *viewer = Viewer;
		grs_canvas *canvas = grd_curcanv;
		fix frame_time = FrameTime;
		int robot = -1;
		std::vector<ubyte> automap{ Automap_visited, Automap_visited + MAX_SEGMENTS };
		~Restore()
		{
			if (robot >= 0) obj_delete(robot);
			ogl_freebmtexture(&bitmap);
			bitmap = saved_bitmap;
			surface = saved_surface;
			ConsoleObject->pos = position;
			ConsoleObject->orient = orientation;
			obj_relink(Players[Player_num].objnum, segment);
			Viewer = viewer;
			FrameTime = frame_time;
			gr_set_current_canvas(canvas);
			std::copy(automap.begin(), automap.end(), Automap_visited);
			xl_compat::bind_skybox_render(original);
		}
	} restore{ original, surface, bitmap, surface, bitmap };
	std::array<ubyte, 4> pixel{ 255, 0, 0, 255 };
	bitmap.bm_type = BM_RGBA;
	bitmap.bm_w = bitmap.bm_h = 1;
	bitmap.bm_rowsize = 4;
	bitmap.bm_flags = 0;
	bitmap.bm_data = pixel.data();
	bitmap.gltexture = nullptr;
	surface.tmap_num2 = 0;
	vms_vector center, direction;
	compute_center_point_on_side(&center, &Segments[selected_segment], selected_side);
	direction = surface.normals[0];
	vm_vec_scale_add(&ConsoleObject->pos, &center, &direction, 4 * F1_0);
	vm_vec_negate(&direction);
	vm_vector_2_matrix(&ConsoleObject->orient, &direction, nullptr, nullptr);
	obj_relink(Players[Player_num].objnum, selected_segment);
	require(!get_seg_masks(&ConsoleObject->pos, selected_segment, 0, nullptr, 0).centermask, "Skybox fixed pose is outside its segment");
	Viewer = ConsoleObject;
	FrameTime = 0;
	gr_set_current_canvas(nullptr);
	auto derived = original;
	for (auto &segment : derived.segments) segment.function = 10;
	xl_compat::bind_skybox_render(derived);
	std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
	for (auto &uvl : surface.uvls) uvl.l = 0;
	skybox_probe_pixels();
	const auto dark_input = skybox_probe_pixels();
	for (auto &uvl : surface.uvls) uvl.l = F1_0;
	const auto bright_input = skybox_probe_pixels();
	require(dark_input == bright_input, "Skybox brightness depends on native static light");
	require(std::all_of(Automap_visited, Automap_visited + MAX_SEGMENTS, [](ubyte value) { return value == 0; }), "Skybox pass marked mine geometry explored");
	unsigned red = 0;
	for (unsigned i = 0; i < bright_input.size(); i += 4)
		red += bright_input[i] >= 250 && bright_input[i + 1] <= 2 && bright_input[i + 2] <= 2;
	require(red >= 900, "Skybox controlled texture was not drawn at full brightness");
	ogl_freebmtexture(&bitmap);
	pixel = { 0, 255, 0, 255 };
	int remote = -1;
	vms_vector remote_center;
	for (int i = 0; i <= Highest_segment_index; ++i) {
		if (std::find(Render_list, Render_list + N_render_segs, i) != Render_list + N_render_segs) continue;
		compute_segment_center(&remote_center, &Segments[i]);
		if (get_seg_masks(&remote_center, i, F1_0, nullptr, 0).centermask) continue;
		remote = i;
		break;
	}
	require(remote >= 0, "Skybox visibility control requires a separate segment outside the ordinary list");
	restore.robot = obj_create(OBJ_ROBOT, 0, remote, &remote_center, &vmd_identity_matrix, F1_0, CT_AI, MT_NONE, RT_POLYOBJ);
	require(restore.robot >= 0, "Skybox visibility control cannot allocate its robot");
	Objects[restore.robot].rtype.pobj_info.model_num = Robot_info[0].model_num;
	const auto green_input = skybox_probe_pixels();
	unsigned green = 0;
	for (unsigned i = 0; i < green_input.size(); i += 4)
		green += green_input[i] <= 2 && green_input[i + 1] >= 250 && green_input[i + 2] <= 2;
	require(green >= 900 && green_input != bright_input, "Skybox pass did not use the current texture pixels");
	require(!xl_skybox_rendering(), "Skybox full-bright state leaked after the frame");
#ifndef NDEBUG
	unsigned objects = 0, outside = 0;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if (object.type == OBJ_NONE || !xl_skybox_segment(object.segnum)) continue;
		require(object_rendered[i] == 1, "Skybox object did not reach the native object renderer");
		++objects;
		outside += std::find(Render_list, Render_list + N_render_segs, object.segnum) == Render_list + N_render_segs;
	}
	require(objects && outside, "Skybox object scene did not distinguish the separate object list");
	con_printf(CON_NORMAL, "XL skybox objects: %u native renderer visits, %u outside ordinary segment list, no duplicate visits\n", objects, outside);
#endif
	const auto compare_visibility = [&] {
		const auto &window = Window_rendered_data[0];
		const std::vector<int> rendered(window.rendered_objects, window.rendered_objects + window.num_objects);
		std::array<short, MAX_RENDERED_OBJECTS> collected{};
		const int count = render_collect_view_objects(0, collected.data());
		require(count == int(rendered.size()) && std::equal(rendered.begin(), rendered.end(), collected.begin()), "Skybox gameplay visibility differs from actual rendered robot/player order");
		require(count >= 2 && std::find(collected.begin(), collected.begin() + count, restore.robot) != collected.begin() + count, "Skybox visibility parity lost its robot outside the ordinary list");
		con_printf(CON_NORMAL, "XL skybox visibility: %d robot/player entries match actual rendered order, including separate-segment robot\n", count);
	};
	compare_visibility();
	skybox_render_depth_controls();
	// A mixed world must retain the ordinary foreground and its discovery
	xl_compat::bind_skybox_render(original);
	const auto ordinary = skybox_probe_pixels();
	for (std::size_t i = 0; i < derived.segments.size(); ++i)
		derived.segments[i].function = int(i) != selected_segment && (i % 2 == 0 || int(i) == remote) ? 10 : 0;
	xl_compat::bind_skybox_render(derived);
	std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
	const auto mixed = skybox_probe_pixels();
	require(mixed == ordinary, "Skybox overwrote ordinary foreground geometry");
	require(Automap_visited[selected_segment] != 0, "Mixed skybox scene stopped ordinary automap discovery");
	for (std::size_t i = 0; i < derived.segments.size(); ++i)
		if (derived.segments[i].function == 10) require(!Automap_visited[i], "Mixed scene explored a skybox segment");
	compare_visibility();
	con_printf(CON_NORMAL, "XL skybox mixed scene: ordinary foreground pixels and discovery preserved, skybox discovery excluded\n");
	con_printf(CON_NORMAL, "XL skybox render scene: fixed pose %d/%d, %u red and %u green pixels; static-light independence and automap isolation passed", selected_segment, selected_side, red, green);
}
