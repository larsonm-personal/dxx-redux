#ifndef DXX_XL_CAMERA_VIEW_POLICY_H
#define DXX_XL_CAMERA_VIEW_POLICY_H
namespace xl_compat
{
struct CameraEffectsInput {
	int stereo_device = 0, stereo_separation = 0, window = 0, quality = 0;
	bool camera = false, automap = false;
	bool map_sparks = false, map_particles = false, map_lightning = false;
	bool particle_aux = false, particle_monitors = false;
	bool lightning_aux = false, lightning_monitors = false;
};
struct CameraEffectsPolicy {
	bool effects = false, sparks = false, particles = false, lightning = false, postprocess = false;
};
// Inputs are resolved render options, not raw profile-file defaults
inline CameraEffectsPolicy camera_effects_policy(const CameraEffectsInput &input)
{
	CameraEffectsPolicy result;
	result.effects = !input.stereo_device || input.stereo_separation < 0 || input.window || input.camera;
	result.postprocess = !input.window;
	if (result.effects) {
		result.sparks = input.automap ? input.map_sparks : input.quality > 0;
		result.particles = input.automap ? input.map_particles : (!input.window || input.particle_aux) && (!input.camera || input.particle_monitors);
		result.lightning = input.automap ? input.map_lightning : (!input.window || input.lightning_aux) && (!input.camera || input.lightning_monitors);
	}
	return result;
}
inline bool camera_discovers_automap(bool active, int viewer_type)
{
	return !active && viewer_type != 2;
}
inline bool camera_monitors_enabled(bool enabled, bool multiplayer, bool remote_options, bool remote_enabled, bool active)
{
	return enabled && (!multiplayer || (remote_options && remote_enabled)) && !active;
}
} // namespace xl_compat
#endif
