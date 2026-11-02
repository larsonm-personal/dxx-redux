// Protected original PMC owner; fixed poses and direct authored exits are not human traversal
nlohmann::json pmc_saved_cameras;
nlohmann::json pmc_saved_lightning;
nlohmann::json pmc_checkpoint_state;
unsigned pmc_contact_baseline = 0;
int pmc_saved_lives = 0;
std::vector<int> pmc_static_owners()
{
	std::vector<int> owners;
	// Fixed owners from the unchanged original level inventories, including melee normalization
	if (Current_level_num == 13) {
		for (int i = 149; i <= 158; ++i) owners.push_back(i);
		owners.push_back(212);
		for (int i = 225; i <= 293; ++i) owners.push_back(i);
	} else {
		const int first = Current_level_num == 11 ? 90 : 181;
		const int last = Current_level_num == 11 ? 100 : 205;
		for (int i = first; i <= last; ++i) owners.push_back(i);
	}
	return owners;
}
std::vector<int> pmc_camera_owners()
{
	require(Current_mission && !std::strcmp(Current_mission_filename, "pmines") && xl_level_extensions_active() &&
	            (Current_level_num == 11 || Current_level_num == 12 || Current_level_num == 13),
	        "PMC control requires an unchanged original PMC level");
	const std::vector<int> expected = Current_level_num == 11 ? std::vector<int>{ 102 } : Current_level_num == 12 ? std::vector<int>{ 206, 207, 208 }
	                                                                                                              : std::vector<int>{ 295, 296, 297, 298, 299 };
	std::vector<int> actual;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == 16) actual.push_back(i);
	require(actual == expected, "Original PMC camera object indices changed");
	for (int owner : actual) {
		const auto &camera = Objects[owner];
		XlCameraControllerState state;
		require(camera.control_type == 18 && camera.movement_type == MT_NONE && camera.render_type == RT_POLYOBJ &&
		            camera.ctype.ai_info.behavior == (owner == 298 ? 129 : owner == 299 ? 135
		                                                                                : 128) &&
		            camera.lifeleft == IMMORTAL_TIME && Robot_info[camera.id].strength == -1 &&
		            xl_camera_controller_get(owner, state),
		        "Original PMC camera lost normalized model, controller or indestructibility");
	}
	for (int owner : pmc_static_owners())
		require(Objects[owner].type == OBJ_ROBOT && Objects[owner].control_type == CT_NONE && Objects[owner].movement_type == MT_NONE &&
		            Objects[owner].ctype.ai_info.behavior == (Robot_info[Objects[owner].id].attack_type ? AIB_NORMAL : 135),
		        "Original PMC static robot lost normalized behavior or acquired movement");
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
	for (int owner : pmc_static_owners()) {
		const auto &robot = Objects[owner];
		rows.push_back({ owner, robot.signature, robot.id, robot.ctype.ai_info.behavior, robot.mtype.phys_info.mass,
		                 robot.pos.x, robot.pos.y, robot.pos.z, robot.mtype.phys_info.velocity.x,
		                 robot.mtype.phys_info.velocity.y, robot.mtype.phys_info.velocity.z });
	}
	return rows;
}
void pmc_playtest_probe(const char *operation)
{
	if (!std::strcmp(operation, "stock")) {
		require(Current_mission && !xl_level_extensions_active(), "PMC stock isolation needs an ordinary mission");
		for (int owner = 295; owner <= 299; ++owner) {
			XlCameraControllerState state;
			require(!xl_camera_controller_get(owner, state), "PMC camera controller leaked into the stock mission");
		}
		for (int i = 0; i <= Highest_object_index; ++i)
			require(Objects[i].type != 16, "PMC camera actor leaked into the stock mission");
		for (int owner : { 320, 322 })
			require(!xl_compat::lightning_object_emitter(owner), "PMC fixed lightning leaked into the stock mission");
		for (int segment = 0; segment < Num_segments; ++segment)
			for (int side = 0; side < 6; ++side)
				require(!xl_camera_monitor_face(segment, side), "PMC camera monitor binding leaked into the stock mission");
		++checks;
		report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "pmc_stock_isolated", true } }.dump();
		return;
	}
	const auto owners = pmc_camera_owners();
	unsigned monitor_count = 0;
	if (!std::strcmp(operation, "checkpoint")) {
		require(Current_level_num == 13 && game_is_time_paused(), "PMC cold checkpoint capture requires paused original PMC3");
		auto rows = pmc_camera_snapshot();
		// Menu restore may advance a frame before pause; rotating orientation is checked separately
		for (std::size_t i = 0; i < owners.size(); ++i)
			if (Objects[owners[i]].ctype.ai_info.behavior == 129) rows[i].erase(rows[i].begin() + 10, rows[i].end());
		const auto &pilot = Players[Player_num];
		pmc_checkpoint_state = { { "mission", std::string(Current_mission_filename) }, { "level", Current_level_num }, { "energy", pilot.energy }, { "shields", pilot.shields }, { "lives", pilot.lives }, { "score", pilot.score }, { "flags", pilot.flags }, { "laser_level", pilot.laser_level }, { "primary_weapons", pilot.primary_weapon_flags }, { "secondary_weapons", pilot.secondary_weapon_flags }, { "actors", rows } };
	} else if (!std::strcmp(operation, "capture")) {
		require(game_is_time_paused(), "PMC capture requires pause");
		pmc_saved_cameras = pmc_camera_snapshot();
		pmc_saved_lightning = xl_compat::lightning_objects_snapshot();
		pmc_saved_lives = Players[Player_num].lives;
	} else if (!std::strcmp(operation, "verify")) {
		require(game_is_time_paused() && !pmc_saved_cameras.empty() && pmc_camera_snapshot() == pmc_saved_cameras, "PMC disk restore changed original camera or static robot state");
		require(Players[Player_num].lives == pmc_saved_lives, "PMC disk restore changed the saved life count");
		require(xl_compat::lightning_objects_snapshot() == pmc_saved_lightning, "PMC disk restore changed lightning activation or identity");
	} else if (!std::strcmp(operation, "restarted")) {
		require(!Player_is_dead && pmc_saved_lives > 1 && Players[Player_num].lives == pmc_saved_lives - 1,
		        "Original PMC restart did not consume exactly one life");
	} else if (!std::strcmp(operation, "live")) {
		require(!game_is_time_paused(), "PMC live control requires gameplay");
		for (int owner : owners) {
			XlCameraControllerState state;
			if (Objects[owner].ctype.ai_info.behavior == 129) {
				require(xl_camera_controller_get(owner, state) && state.rotation.delta != 0, "Original rotating PMC camera did not advance");
				continue;
			}
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
		const std::vector<std::array<int, 2>> faces = Current_level_num == 11 ? std::vector<std::array<int, 2>>{ { 719, 4 } } : Current_level_num == 12 ? std::vector<std::array<int, 2>>{ { 73, 0 }, { 65, 2 }, { 167, 0 }, { 159, 2 }, { 261, 0 }, { 253, 2 } }
		                                                                                                                                                : std::vector<std::array<int, 2>>{ { 3179, 2 }, { 3177, 2 }, { 3126, 2 }, { 3178, 2 }, { 3132, 0 } };
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
		const auto &camera = Objects[Current_level_num == 13 ? owners.back() : owners.front()];
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
		require(Current_level_num == 12 && xl_level_supported_at("pmlev3.rl2", 0, 13), "Qualified original PMC3 is not ready for the authored exit");
	} else if (!std::strcmp(operation, "end_ready")) {
		require(Current_level_num == 13 && Last_level == 14 && Num_triggers > 38 && Triggers[38].type == TT_EXIT &&
		            Triggers[38].value == 15 * F1_0 && !(Triggers[38].flags & TF_DISABLED),
		        "Original PMC ending destination changed");
	} else if (!std::strcmp(operation, "lightning")) {
		require(Current_level_num == 13 && !game_is_time_paused(), "Fixed lightning needs live original PMC3");
		for (int owner : { 320, 322 }) {
			const auto *emitter = xl_compat::lightning_object_emitter(owner);
			require(emitter && emitter->enabled && emitter->bolts.size() == 5, "Original fixed lightning lost its five parent bolts");
			for (unsigned i = 0; i < emitter->bolts.size(); ++i) {
				const auto *frame = xl_compat::lightning_object_render_frame(owner, i);
				require(emitter->bolts[i].definition.length == 75 * F1_0 && frame && !frame->core.vertices.empty(),
				        "Original fixed lightning lost target distance or visible geometry");
			}
		}
	} else throw std::runtime_error("Unknown original PMC operation");
	++checks;
	auto state = nlohmann::json{ { "passed", true }, { "checks", checks }, { "pmc_level", Current_level_num }, { "pmc_cameras", owners.size() }, { "pmc_monitors", monitor_count }, { "pmc_contacts", xl_camera_player_weapon_contacts() } };
	if (!pmc_checkpoint_state.is_null()) state["pmc_checkpoint"] = pmc_checkpoint_state;
	report = state.dump();
}
