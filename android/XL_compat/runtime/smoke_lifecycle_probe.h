// Original hub smoke and assets; only outdoors is changed in this derived mission
std::vector<unsigned char> smoke_earned_pixels;
nlohmann::json smoke_earned_activation;
void smoke_lifecycle_probe(const char *operation)
{
	require(Game_wind && xl_level_extensions_active() && !std::strcmp(Current_mission_filename, "psmoke") && Current_level_num == 1, "Smoke control requires the derived hub");
	const auto *image = xl_prepared_particle_image(0);
	require(image && image->bitmap.width == 512 && image->bitmap.height == 512 && !xl_prepared_particle_image(), "Original smoke image ownership differs");
	if (!std::strcmp(operation, "capture")) {
		require(skybox_earned_level.objects.size() > 1 && skybox_earned_level.objects[1].segment == 59 && skybox_earned_level.objects[1].render_data == std::vector<int>{ 4, 10, 3, 4, 8, 10, 203, 78, 78, 0, 0, 0, 1 }, "Derived hub changed original smoke payload");
		smoke_earned_pixels = image->bitmap.pixels;
		smoke_earned_activation = xl_compat::particle_objects_snapshot();
		require(smoke_earned_activation.size() == 1 && smoke_earned_activation[0][0] == 1 && smoke_earned_activation[0][2] == true, "Original smoke owner is not enabled");
	} else if (!std::strcmp(operation, "verify") || !std::strcmp(operation, "gpu")) {
		require(!smoke_earned_pixels.empty() && image->bitmap.pixels == smoke_earned_pixels && xl_compat::particle_objects_snapshot() == smoke_earned_activation, "Smoke image or activation changed after lifecycle");
		if (!std::strcmp(operation, "gpu")) particle_gpu_probe(image, 8, true);
	} else if (!std::strcmp(operation, "live") || !std::strcmp(operation, "scene")) {
		unsigned particles = 0;
		for (const auto &system : xl_compat::particle_systems()) {
			require(system.object == 1 && std::holds_alternative<xl_compat::SmokeEmitter>(system.emitter), "Original smoke used the wrong runtime emitter");
			particles += system.count();
		}
		require(particles > 0, "Original smoke emitted no live particles");
		if (!std::strcmp(operation, "scene")) xl_particle_scene_probe_request();
	} else throw std::runtime_error("Unknown smoke lifecycle operation");
}
