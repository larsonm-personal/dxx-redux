// Protected original PMC owner; fixed poses and direct authored exits are not human traversal
nlohmann::json pmc_saved_cameras;
unsigned pmc_contact_baseline = 0;
int pmc_saved_lives = 0;
std::vector<int> pmc_camera_owners()
{
	require(Current_mission && !std::strcmp(Current_mission_filename, "pmines") && xl_level_extensions_active() &&
	            (Current_level_num == 11 || Current_level_num == 12),
	        "PMC control requires an unchanged original PMC level");
	const std::vector<int> expected = Current_level_num == 11 ? std::vector<int>{ 102 } : std::vector<int>{ 206, 207, 208 };
	std::vector<int> actual;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == 16) actual.push_back(i);
	require(actual == expected, "Original PMC camera object indices changed");
	for (int owner : actual) {
		const auto &camera = Objects[owner];
		XlCameraControllerState state;
		require(camera.control_type == 18 && camera.movement_type == MT_NONE && camera.render_type == RT_POLYOBJ &&
		            camera.ctype.ai_info.behavior == 128 && camera.lifeleft == IMMORTAL_TIME && Robot_info[camera.id].strength == -1 &&
		            xl_camera_controller_get(owner, state),
		        "Original PMC camera lost normalized model, controller or indestructibility");
	}
	const int first = Current_level_num == 11 ? 90 : 181;
	const int last = Current_level_num == 11 ? 100 : 205;
	for (int owner = first; owner <= last; ++owner)
		require(Objects[owner].type == OBJ_ROBOT && Objects[owner].control_type == CT_NONE && Objects[owner].movement_type == MT_NONE,
		        "Original PMC static robot acquired AI or physics movement");
	return actual;
}
nlohmann::json pmc_camera_snapshot()
{
	nlohmann::json rows = nlohmann::json::array();
	for (int owner : pmc_camera_owners()) {
		const auto &camera = Objects[owner];
		const auto &m = camera.orient;
		rows.push_back({ owner, camera.signature, camera.id, camera.rtype.pobj_info.model_num, camera.size, camera.shields, camera.segnum,
		                 camera.pos.x, camera.pos.y, camera.pos.z, m.rvec.x, m.rvec.y, m.rvec.z, m.uvec.x, m.uvec.y, m.uvec.z, m.fvec.x, m.fvec.y, m.fvec.z });
	}
	return rows;
}
void pmc_playtest_probe(const char *operation)
{
	const auto owners = pmc_camera_owners();
	unsigned monitor_count = 0;
	if (!std::strcmp(operation, "capture")) {
		require(game_is_time_paused(), "PMC capture requires pause");
		pmc_saved_cameras = pmc_camera_snapshot();
		pmc_saved_lives = Players[Player_num].lives;
	} else if (!std::strcmp(operation, "verify")) {
		require(game_is_time_paused() && !pmc_saved_cameras.empty() && pmc_camera_snapshot() == pmc_saved_cameras, "PMC disk restore changed original camera identities, geometry or orientation");
		require(Players[Player_num].lives == pmc_saved_lives, "PMC disk restore changed the saved life count");
	} else if (!std::strcmp(operation, "restarted")) {
		require(!Player_is_dead && pmc_saved_lives > 1 && Players[Player_num].lives == pmc_saved_lives - 1,
		        "Original PMC restart did not consume exactly one life");
	} else if (!std::strcmp(operation, "live")) {
		require(!game_is_time_paused(), "PMC live control requires gameplay");
		for (int owner : owners) {
			XlCameraControllerState state;
			require(xl_camera_controller_get(owner, state) && state.rotation.angle == 0 && state.rotation.delta == 0,
			        "Original still PMC camera acquired a rotating controller");
			const auto &orientation = Objects[owner].orient;
			require(state.original == xl_compat::CameraMatrix{ { { orientation.rvec.x, orientation.rvec.y, orientation.rvec.z },
			                                                     { orientation.uvec.x, orientation.uvec.y, orientation.uvec.z },
			                                                     { orientation.fvec.x, orientation.fvec.y, orientation.fvec.z } } },
			        "Original still PMC camera changed its authored orientation");
		}
	} else if (!std::strcmp(operation, "monitor")) {
		require(game_is_time_paused() && gl_initialized, "PMC monitor requires a paused real GL context");
		xl_compat::LightningGlowState restore_gl;
		Readback readback;
		CameraTargetPattern sampler;
		const std::vector<std::array<int, 2>> faces = Current_level_num == 11 ? std::vector<std::array<int, 2>>{ { 719, 4 } } : std::vector<std::array<int, 2>>{ { 73, 0 }, { 65, 2 }, { 167, 0 }, { 159, 2 }, { 261, 0 }, { 253, 2 } };
		for (const auto &face : faces) {
			require(xl_camera_monitor_face(face[0], face[1]), "Original PMC monitor binding missing");
			int vertices[4];
			get_side_verts(vertices, face[0], face[1]);
			const int overlay = Segments[face[0]].sides[face[1]].tmap_num2;
			grs_bitmap original{};
			grs_bitmap *image = nullptr;
			for (int attempt = 0; attempt < 8; ++attempt) {
				xl_camera_renderer_frame();
				xl_camera_monitor_begin(face[0], face[1], 4, vertices, overlay);
				image = xl_camera_monitor_bitmap(0, &original);
				if (image == &original && overlay) image = xl_camera_monitor_bitmap(1, &original);
				xl_camera_monitor_end();
				if (image != &original) break;
			}
			require(image && image != &original && image->gltexture && image->bm_w > 0 && image->bm_h > 0, "Original PMC monitor never captured its camera view");
			const auto pixels = sampler.sample(image->gltexture->handle, image->bm_w, image->bm_h, 0);
			unsigned visible = 0;
			for (std::size_t i = 0; i < pixels.size(); i += 4) {
				require(pixels[i + 3] == 255, "Original camera monitor lost opacity");
				visible += pixels[i] > 8 || pixels[i + 1] > 8 || pixels[i + 2] > 8;
			}
			require(visible > pixels.size() / 400, "Original PMC monitor lacks visible scene information");
			++monitor_count;
		}
		require(glGetError() == GL_NO_ERROR, "Original PMC monitor raised a GL error");
	} else if (!std::strcmp(operation, "aim_camera")) {
		require(!game_is_time_paused(), "Camera firing pose requires live gameplay");
		const auto &camera = Objects[owners.front()];
		vms_vector position, direction = camera.orient.fvec;
		vm_vec_scale_add(&position, &camera.pos, &direction, camera.size + ConsoleObject->size + 4 * F1_0);
		const int room = find_point_seg(&position, camera.segnum);
		require(room >= 0, "Original camera firing pose is outside the mine");
		obj_relink(int(ConsoleObject - Objects), room);
		ConsoleObject->pos = ConsoleObject->last_pos = position;
		vm_vec_negate(&direction);
		vm_vector_2_matrix(&ConsoleObject->orient, &direction, nullptr, nullptr);
		ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = {};
		pmc_contact_baseline = xl_camera_player_weapon_contacts();
	} else if (!std::strcmp(operation, "fired_camera")) {
		require(xl_camera_player_weapon_contacts() > pmc_contact_baseline, "Ordinary player laser input did not reach the original camera");
	} else if (!std::strcmp(operation, "next_gate")) {
		require(Current_level_num == 12 && !xl_level_supported_at("pmlev3.rl2", 0, 13), "Unqualified PMC3 must remain blocked");
	} else throw std::runtime_error("Unknown original PMC operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "pmc_level", Current_level_num }, { "pmc_cameras", owners.size() }, { "pmc_monitors", monitor_count }, { "pmc_contacts", xl_camera_player_weapon_contacts() } }.dump();
}
