#include "oof_resource.h"
#include "../../app/src/main/cpp/shared/mod_save_compat.h"
void mod_identity_save();
bool mod_identity_restore();
static std::unique_ptr<XlPreparedResources> mod_prepare_level(int ordinal)
{
	const char *name = Level_names[ordinal - 1];
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(PHYSFS_openRead(name), PHYSFS_close);
	require(bool(input), "Cannot open original level for resource transition check");
	const auto size = PHYSFS_fileLength(input.get());
	require(size > 0 && size <= 64 * 1024 * 1024, "Invalid original level size");
	xl_compat::Bytes bytes(static_cast<std::size_t>(size));
	require(PHYSFS_read(input.get(), bytes.data(), 1, static_cast<PHYSFS_uint32>(bytes.size())) == size,
	        "Cannot read original level for resource transition check");
	const auto level = xl_compat::inspect_level(bytes);
	auto prepared = std::make_unique<XlPreparedResources>(name, &level, ordinal);
	if (ordinal == 13) {
		for (const auto &object : level.objects)
			if (object.type == OBJ_ROBOT && (object.id == 31 || object.id == 64 || object.id == 65 || object.id == 72))
				prepared->dependencies().robot_behavior(object.id, object.control_data.at(0), level.level_version >= 21);
		for (int id : { 31, 64, 72 }) {
			bool rejected = false;
			try {
				prepared->dependencies().robot(id);
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			require(rejected, "Level HXM static robot incorrectly admitted as an animated robot");
		}
	}
	return prepared;
}
// Original mission files are staged by the caller into an owned disposable root
static std::vector<unsigned char> mod_render_definitions()
{
	std::vector<unsigned char> result;
	auto append = [&](const auto *data, int count) {
		if (!count) return;
		const auto *bytes = reinterpret_cast<const unsigned char *>(data);
		result.insert(result.end(), bytes, bytes + sizeof(*data) * count);
	};
	append(&NumTextures, 1);
	append(Textures, NumTextures);
	append(TmapInfo, NumTextures);
	append(Sounds, MAX_SOUNDS);
	append(AltSounds, MAX_SOUNDS);
	append(&N_ObjBitmaps, 1);
	append(ObjBitmaps, N_ObjBitmaps);
	append(ObjBitmapPtrs, N_ObjBitmaps);
	append(&N_polygon_models, 1);
	append(Dying_modelnums, N_polygon_models);
	append(Dead_modelnums, N_polygon_models);
	for (int id = 0; id < N_polygon_models; ++id) {
		auto model = Polygon_models[id];
		model.model_data = nullptr;
		append(&model, 1);
		append(Polygon_models[id].model_data, model.model_data_size);
	}
	append(Player_ship, 1);
	return result;
}

static int mod_data_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("d2", error, sizeof(error)), error);
		const auto stock = gameplay_definitions();
		const auto stock_render = mod_render_definitions();
		const int stock_models = N_polygon_models;
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		std::map<std::string, std::string> original_sources;
		for (int cycle = 0; cycle < 2; ++cycle) {
			require(load_base_mission("pmines", error, sizeof(error)), error);
			require(!*xl_resource_base_rejection(), xl_resource_base_rejection());
			require(N_robot_types == 78 && Robot_info[68].model_num == 17,
			        "Original Pumo V-HAM robot68 was not loaded (must not use Vertigo model168)");
			require(N_weapon_types == 65, "Original Pumo extension weapon count differs");
			const auto ase_model = xl_owned_oof_model(17);
			require(ase_model && ase_model->path == "mods/pmines/models/model17.ase" && ase_model->skins.size() == 3 &&
			            !ase_model->mesh.faces.empty(),
			        "Original Pumo ASE model and owned skins were not staged");
			for (int id : { 170, 172, 173, 174 })
				require(xl_owned_oof_model(id) && Polygon_models[id].model_data, "Missing ASE-only native model geometry");
			const auto ship_model = xl_owned_oof_model(108);
			require(ship_model && ship_model->path == "mods/pmines/models/pyrogl.ase" && ship_model->gunpoints.size() == 8 &&
			            xl_owned_oof_model(109) == ship_model && xl_owned_oof_model(110) == ship_model,
			        "Named player ASE or shared file ownership was not loaded");
			require(xl_owned_oof_model(127) && xl_owned_oof_model(127)->path == "mods/pmines/models/concussion.ase" &&
			            xl_owned_oof_model(137) == xl_owned_oof_model(127),
			        "Source alias slot did not share named missile model");
			const auto &sources = xl_resource_mod_sources();
			require(sources.count("mods/pmines/pmines.ham") && sources.count("mods/pmines/pmines.hxm") &&
			            sources.count("mods/pmines/pmines.vham") && sources.count("mods/pmines/models/model171.oof") &&
			            sources.count("mods/pmines/models/emberbody.tga"),
			        "XL save identity omits mission resources");
			if (!cycle) original_sources = sources;
			else require(sources == original_sources, "Unchanged mission source identity is unstable");
			const auto owned_model = xl_owned_oof_model(171);
			require(owned_model && owned_model->model.submodels.size() == 5 && owned_model->skins.size() == 3 &&
			            owned_model->skins[0]->width == 128,
			        "Original Pumo OOF model and owned skins were not staged");
			xl_assets::ObjectDependencies graph;
			graph.xl_boss_spawning = true;
			graph.robot_behavior(68, 135, true);
			bool animated_rejected = false;
			try {
				graph.robot(68);
			} catch (const std::runtime_error &) {
				animated_rejected = true;
			}
			require(animated_rejected, "Static admission incorrectly qualified missing animation joints for an active robot");
			const auto custom = gameplay_definitions();
			require(custom != stock, "Mission HAM/HXM/V-HAM did not change definitions");
			require(load_base_mission("d2", error, sizeof(error)), error);
			require(gameplay_definitions() == stock && N_polygon_models == stock_models,
			        "XL custom definitions leaked into stock mission");
			require(mod_render_definitions() == stock_render, "XL models, textures or sound mappings leaked into stock mission");
			require(!xl_owned_oof_model(171), "XL OOF model survived stock selection");
			require(xl_resource_mod_sources().empty(), "XL source identity survived stock selection");
		}
		std::cout << "Original Pumo HAM/HXM/V-HAM loading and stock round-trip passed\n";
		{
			using Input = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
			Input ham(PHYSFS_openRead("mods/pmines/pmines.ham"), PHYSFS_close);
			XlPreparedResources target{ XlPreparedHam(ham.get()) };
			Input hxm(PHYSFS_openRead("mods/pmines/pmines.hxm"), PHYSFS_close);
			target.prepare_hxm(hxm.get());
			Input vham(PHYSFS_openRead("mods/pmines/pmines.vham"), PHYSFS_close);
			const auto hash = target.prepare_vham(vham.get());
			vham.reset();
			require(hash == original_sources.at("mods/pmines/pmines.vham") &&
			            target.dependencies().robot_count == 78 && target.dependencies().robot_data[68].model_num == 17 &&
			            gameplay_definitions() == stock && mod_render_definitions() == stock_render,
			        "Incoming VHAM borrowed or modified the outgoing mission definitions");
			const auto path = root / "mods/pmines/pmines.vham";
			std::ifstream input(path, std::ios::binary);
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			bool rejected = false;
			try {
				Input invalid(PHYSFS_openRead("mods/pmines/pmines.vham"), PHYSFS_close);
				target.prepare_vham(invalid.get());
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			require(rejected && target.dependencies().robot_data[68].model_num == 17 &&
			            gameplay_definitions() == stock && mod_render_definitions() == stock_render,
			        "Malformed VHAM changed retained target or outgoing state");
			target.install_hxm();
			require(N_robot_types == 78 && N_weapon_types == 65 && Robot_info[68].model_num == 17,
			        "VHAM publication reread the changed source instead of retained definitions");
			write_bytes(path.string().c_str(), original);
			require(load_base_mission("d2", error, sizeof(error)), error);
			require(gameplay_definitions() == stock && mod_render_definitions() == stock_render,
			        "Retained VHAM publication leaked through stock mission reload");
		}
		std::cout << "Retained incoming VHAM: source replacement, failed retry and native publication passed\n";

		require(load_base_mission("pmines", error, sizeof(error)), error);
		{
			const auto installed_ship = xl_owned_oof_model(108);
			for (int ordinal = 1; ordinal <= Last_level; ++ordinal) {
				const auto staged = xl_prepare_owned_models(ordinal);
				require(!staged.models.empty() && !staged.sources.empty(), "Level model preparation returned no resources");
				if (ordinal == 2) require(staged.models.at(108)->path == "mods/pmines/models/level02/pyrogl.ase", "Level ship did not override mission ship");
				if (ordinal == 9) require(staged.models.at(49)->gunpoints.size() == 3, "Level robot gunpoints were not staged");
				if (ordinal == 11) require(staged.models.at(99)->path == "mods/pmines/models/level11/model99.ase", "Level reactor override was not staged");
				require(xl_owned_oof_model(108) == installed_ship && xl_resource_mod_sources() == original_sources,
				        "Preparing another level mutated installed model resources or save identity");
			}
			const auto path = root / "mods/pmines/models/level11/model99.ase";
			std::ifstream input(path, std::ios::binary);
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			bool rejected = false;
			try {
				(void) xl_prepare_owned_models(11);
			} catch (const std::exception &) {
				rejected = true;
			}
			write_bytes(path.string().c_str(), original);
			require(rejected && xl_owned_oof_model(108) == installed_ship && xl_resource_mod_sources() == original_sources,
			        "Failed level model preparation changed installed resources");
		}
		require(load_base_mission("d2", error, sizeof(error)), error);
		std::cout << "All Pumo level model sets stage without mutating installed resources; malformed override rejects\n";
		require(load_base_mission("pmines", error, sizeof(error)), error);
		{
			std::vector<unsigned char> base_geometry;
			for (int ordinal : { 2, 13, 11, 2 }) {
				const auto before = mod_render_definitions();
				const auto sources = xl_resource_mod_sources();
				auto prepared = mod_prepare_level(ordinal);
				require(mod_render_definitions() == before && xl_resource_mod_sources() == sources,
				        "Level preparation changed installed native definitions");
				xl_set_prepared_resources(Level_names[ordinal - 1], std::move(prepared));
				require(xl_install_prepared_hxm(Level_names[ordinal - 1]), "Prepared level resources were not installed");
				require(xl_resource_mod_sources() == xl_resource_sources_for_level(ordinal), "Installed target save identity differs");
				const auto ship = xl_owned_oof_model(108);
				require(ship && ship->path == (ordinal == 2 ? "mods/pmines/models/level02/pyrogl.ase" : "mods/pmines/models/pyrogl.ase"),
				        "Level ship override did not install or restore");
				const auto &model = Polygon_models[172];
				const std::vector<unsigned char> geometry(model.model_data, model.model_data + model.model_data_size);
				if (base_geometry.empty()) base_geometry = geometry;
				else require((geometry == base_geometry) == (ordinal != 13), "Generated model geometry did not change or restore across levels");
			}
			const auto before = mod_render_definitions();
			const auto sources = xl_resource_mod_sources();
			const auto ship = xl_owned_oof_model(108);
			const auto path = root / "mods/pmines/models/level11/model99.ase";
			std::ifstream input(path, std::ios::binary);
			require(bool(input), "Missing rejected-preparation fixture");
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			bool rejected = false;
			try {
				(void) mod_prepare_level(11);
			} catch (const std::exception &) {
				rejected = true;
			}
			write_bytes(path.string().c_str(), original);
			require(rejected && mod_render_definitions() == before && xl_resource_mod_sources() == sources && xl_owned_oof_model(108) == ship,
			        "Rejected full level preparation changed installed resources");
		}
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(gameplay_definitions() == stock && mod_render_definitions() == stock_render && xl_resource_mod_sources().empty(),
		        "Installed XL level resources leaked into stock mission");
		std::cout << "Consecutive installed level overrides, generated geometry and stock restoration passed\n";
		{
			const auto foreign = root / "owner-shadow";
			const auto shadow = foreign / "mods/pmines/pmines.ham";
			std::filesystem::create_directories(shadow.parent_path());
			write_bytes(shadow.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			require(PHYSFS_mount(foreign.string().c_str(), nullptr, 0), "Cannot mount owned foreign-origin fixture");
			try {
				require(load_base_mission("d2", error, sizeof(error)), error);
				require(!*xl_resource_base_rejection() && gameplay_definitions() == stock && mod_render_definitions() == stock_render,
				        "Unrelated XL resource changed stock loading");
				require(load_base_mission("pmines", error, sizeof(error)), error);
				require(std::string(xl_resource_base_rejection()).find("shadowed outside their owner") != std::string::npos,
				        "Foreign mission HAM did not fail its ownership check");
				require(!xl_level_supported("pmlev1.rl2", 0) && xl_resource_mod_sources().empty(),
				        "Foreign mission HAM reached level admission or save identity");
				require(gameplay_definitions() == stock && mod_render_definitions() == stock_render,
				        "Foreign mission HAM changed native definitions");
			} catch (...) {
				PHYSFS_unmount(foreign.string().c_str());
				std::filesystem::remove(shadow);
				throw;
			}
			require(PHYSFS_unmount(foreign.string().c_str()), "Cannot unmount owned foreign-origin fixture");
			std::filesystem::remove(shadow);
			require(load_base_mission("pmines", error, sizeof(error)), error);
			require(!*xl_resource_base_rejection() && xl_resource_mod_sources() == original_sources,
			        "Mission did not recover after foreign-origin removal");
			require(load_base_mission("d2", error, sizeof(error)), error);
		}
		std::cout << "Foreign-origin XL shadow rejected; stock unaffected and original mission recovers\n";
		for (const char *relative : { "mods/pmines/pmines.ham", "mods/pmines/pmines.hxm", "mods/pmines/pmines.vham",
		                              "mods/pmines/models/model171.oof", "mods/pmines/models/emberbody.tga",
		                              "mods/pmines/models/model17.ase" }) {
			const auto path = root / relative;
			std::ifstream input(path, std::ios::binary);
			require(bool(input), "Missing owned malformed-layer test fixture");
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			try {
				require(load_base_mission("pmines", error, sizeof(error)), error);
			} catch (...) {
				write_bytes(path.string().c_str(), original);
				throw;
			}
			write_bytes(path.string().c_str(), original);
			// Mission selection returns descriptor success; resource rejection gates level admission
			require(*xl_resource_base_rejection(), "Malformed XL layer did not record resource rejection");
			require(!xl_level_supported("pmlev1.rl2", 0), "Malformed XL layer was admitted to a level");
			require(xl_resource_mod_sources().empty(), "Rejected XL layer retained save identity");
			require(gameplay_definitions() == stock && mod_render_definitions() == stock_render && !xl_owned_oof_model(171),
			        "Rejected XL layer retained partial custom definitions");
			require(load_base_mission("d2", error, sizeof(error)), error);
			require(gameplay_definitions() == stock && mod_render_definitions() == stock_render,
			        "Stock load after rejected XL layer changed definitions");
			require(load_base_mission("pmines", error, sizeof(error)), error);
			require(N_robot_types == 78 && xl_owned_oof_model(171), "Valid XL load did not recover after rejected layer");
			require(load_base_mission("d2", error, sizeof(error)), error);
		}
		std::cout << "Malformed HAM/HXM/VHAM/OOF/skin rejection, stock isolation and valid recovery passed\n";
		require(load_base_mission("pmines", error, sizeof(error)), error);
		mod_identity_save();
		require(mod_identity_restore(), mod_save_reason());
		{
			const auto path = root / "mods/pmines/models/emberbody.tga";
			std::ifstream input(path, std::ios::binary);
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			require(!original.empty(), "Missing source identity fixture");
			auto changed = original;
			changed.back() ^= 1;
			write_bytes(path.string().c_str(), changed);
			try {
				require(load_base_mission("pmines", error, sizeof(error)), error);
				require(!*xl_resource_base_rejection(), xl_resource_base_rejection());
				require(xl_resource_mod_sources() != original_sources, "Changed owned skin retained original save identity");
				// Restore preflight reads the target level's current files before installation
				require(!mod_identity_restore(), "Save admitted changed XL model skin");
				require(std::string(mod_save_reason()).find("XL mission definitions or models changed") != std::string::npos,
				        "Changed XL save rejected for an unrelated reason");
			} catch (...) {
				write_bytes(path.string().c_str(), original);
				throw;
			}
			write_bytes(path.string().c_str(), original);
		}
		require(load_base_mission("pmines", error, sizeof(error)), error);
		require(xl_resource_mod_sources() == original_sources, "Restored mission did not recover original save identity");
		require(mod_identity_restore(), mod_save_reason());
		std::cout << "XL save admission accepts unchanged assets, rejects changed skin, and recovers original identity\n";
		{
			const auto path = root / "mods/pmines/models/model17.ase";
			std::ifstream input(path, std::ios::binary);
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			const std::string marker = R"ASE(
*GEOMOBJECT {
*NODE_NAME "$gunpnt-0"
*MATERIAL_REF 0
*MESH {
*MESH_NUMVERTEX 3
*MESH_NUMFACES 1
*MESH_NUMTVERTEX 0
*MESH_VERTEX_LIST {
*MESH_VERTEX 0 1 2 3
*MESH_VERTEX 1 3 2 3
*MESH_VERTEX 2 1 4 3
}
*MESH_FACE_LIST {
*MESH_FACE 0: A: 0 B: 1 C: 2
}
*MESH_NORMALS {
*MESH_FACENORMAL 0 0 0 1
*MESH_VERTEXNORMAL 0 0 0 1
*MESH_VERTEXNORMAL 1 0 0 1
*MESH_VERTEXNORMAL 2 0 0 1
}
}
}
)ASE";
			auto changed = original;
			changed.insert(changed.end(), marker.begin(), marker.end());
			write_bytes(path.string().c_str(), changed);
			try {
				require(load_base_mission("pmines", error, sizeof(error)), error);
			} catch (...) {
				write_bytes(path.string().c_str(), original);
				throw;
			}
			write_bytes(path.string().c_str(), original);
			require(!*xl_resource_base_rejection(), xl_resource_base_rejection());
			vms_vector local{};
			require(xl_model_gun_point(17, 0, &local) && local.x == 2 * F1_0 && local.y == 3 * F1_0 && local.z == -3 * F1_0,
			        "Owned ASE gunpoint was not selected");
			const auto original_robot = Robot_info[68];
			const auto original_model = Polygon_models[17];
			Robot_info[68].n_guns = 1;
			Robot_info[68].gun_submodels[0] = 1;
			Polygon_models[17].submodel_parents[1] = 0;
			Polygon_models[17].submodel_offsets[1] = { 4 * F1_0, 5 * F1_0, 6 * F1_0 };
			object robot{};
			robot.id = 68;
			robot.render_type = RT_POLYOBJ;
			robot.rtype.pobj_info.model_num = 17;
			robot.orient = vmd_identity_matrix;
			robot.pos = { 10 * F1_0, 20 * F1_0, 30 * F1_0 };
			vms_vector world{};
			calc_gun_point(&world, &robot, 0);
			Robot_info[68] = original_robot;
			Polygon_models[17] = original_model;
			require(world.x == 16 * F1_0 && world.y == 28 * F1_0 && world.z == 33 * F1_0,
			        "ASE gunpoint skipped native robot submodel or world transforms");
			require(load_base_mission("d2", error, sizeof(error)), error);
			local = { 7, 8, 9 };
			require(!xl_model_gun_point(17, 0, &local) && local.x == 7 && local.y == 8 && local.z == 9,
			        "XL gunpoint changed stock fallback");
			require(load_base_mission("pmines", error, sizeof(error)), error);
			require(xl_resource_mod_sources() == original_sources, "Gunpoint fixture did not restore original identity");
		}
		std::cout << "ASE gunpoint selection, native robot transforms and stock fallback passed\n";
		for (int id = 0; id < N_weapon_types; ++id) {
			const auto &weapon = Weapon_info[id];
			if (weapon.children < 0 && id != 41) continue;
			std::cout << "Pumo weapon " << id << " children=" << int(weapon.children)
			          << " radius=" << weapon.damage_radius << " flash=" << int(weapon.flash)
			          << " strength=" << weapon.strength[0] << " speed=" << weapon.speed[0]
			          << " homing=" << int(weapon.homing_flag) << " bounce=" << int(weapon.bounce) << '\n';
		}
		xl_assets::ObjectDependencies weapons;
		for (int id : Primary_weapon_to_weapon_info) {
			try {
				weapons.weapon(id);
			} catch (const std::exception &error) {
				throw std::runtime_error("Primary weapon " + std::to_string(id) + ": " + error.what());
			}
		}
		for (int id : Secondary_weapon_to_weapon_info) {
			try {
				weapons.weapon(id);
			} catch (const std::exception &error) {
				throw std::runtime_error("Secondary weapon " + std::to_string(id) + ": " + error.what());
			}
		}
		if (find_arg_value(argc, argv, "-mod-data-render-preflight")) {
			const bool level_supported = xl_level_supported("pmlev1.rl2", 0);
			require(level_supported, xl_level_rejection());
		}
		std::cout << "Pumo CPU loading controls passed; rendered startup is a separate required check\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL mod-data controls failed: " << error.what() << '\n';
		return 1;
	}
}
