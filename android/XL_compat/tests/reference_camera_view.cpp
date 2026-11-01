#include "camera_view_policy.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace camera_view_source
{
#define ENTER(...) ((void) 0)
#define PROF_START
#define PROF_END(...)
#define RETURN        return;
#define G3_SLEEP(...) ((void) 0)
std::vector<int> trace;
struct {
	int device = 0, separation = 0;
	int StereoDevice()
	{
		return device;
	}
	int StereoSeparation()
	{
		return separation;
	}
} ogl;
struct {
	bool active = false;
	bool Active()
	{
		return active;
	}
} automap;
struct {
	struct {
		int nThreads = 1;
		bool bHaveExtraGameInfo[2]{};
	} app;
	struct {
		struct {
			bool bActive = false;
		} cameras;
		bool bDoCameras = false;
	} render;
} gameStates;
struct Options {
	struct {
		int nQuality = 0;
		struct {
			bool bSparks = false, bParticles = false, bLightning = false;
		} automap;
		struct {
			bool bAuxViews = false, bMonitors = false;
		} particles, lightning;
	} render;
} gameOptions[2];
Options *gameOpts = gameOptions;
struct Manager {
	int marker;
	void Render()
	{
		trace.push_back(marker);
	}
} sparkManager{ 1 }, particleManager{ 2 }, lightningManager{ 3 };
struct {
	int waits = 0;
	bool Ready()
	{
		trace.push_back(0);
		return waits-- <= 0;
	}
	void Render(int window)
	{
		trace.push_back(10 + window);
	}
} transparencyRenderer;
struct {
	void Prepare()
	{
		trace.push_back(20);
	}
} postProcessManager;
struct {
	bool bUseCameras = false;
} extraGameInfo[2];
bool IsMultiGame = false;
struct Object {
	struct {
		int nType = 0;
	} info;
} viewer;
struct {
	struct {
		Object *pViewer = &viewer;
	} objData;
} gameData;
constexpr int OBJ_ROBOT = 2;
#include "xl_reference_camera_view.h"
#undef ENTER
#undef PROF_START
#undef PROF_END
#undef RETURN
#undef G3_SLEEP
} // namespace camera_view_source
void camera_view_source_controls()
{
	using namespace camera_view_source;
	unsigned cases = 0, camera_cases = 0, visibility_cases = 0;
	for (unsigned mask = 0; mask < 512; ++mask)
		for (int stereo : { -1, 0, 1 })
			for (int separation : { -65536, 0, 65536 })
				for (int window : { 0, 1, 2 })
					for (int quality : { 0, 1, 3 }) {
						xl_compat::CameraEffectsInput input;
						input.stereo_device = ogl.device = stereo;
						input.stereo_separation = ogl.separation = separation;
						input.window = window;
						input.quality = gameOptions[0].render.nQuality = quality;
						input.camera = gameStates.render.cameras.bActive = (mask & 1) != 0;
						input.automap = automap.active = (mask & 2) != 0;
						input.map_sparks = gameOpts->render.automap.bSparks = (mask & 4) != 0;
						input.map_particles = gameOpts->render.automap.bParticles = (mask & 8) != 0;
						input.map_lightning = gameOpts->render.automap.bLightning = (mask & 16) != 0;
						input.particle_aux = gameOpts->render.particles.bAuxViews = (mask & 32) != 0;
						input.particle_monitors = gameOpts->render.particles.bMonitors = (mask & 64) != 0;
						input.lightning_aux = gameOpts->render.lightning.bAuxViews = (mask & 128) != 0;
						input.lightning_monitors = gameOpts->render.lightning.bMonitors = (mask & 256) != 0;
						gameStates.app.nThreads = (mask % 3) + 1;
						transparencyRenderer.waits = int(mask % 4);
						const auto policy = xl_compat::camera_effects_policy(input);
						std::vector<int> expected;
						if (policy.effects && gameStates.app.nThreads > 1)
							for (int n = 0; n <= transparencyRenderer.waits; ++n) expected.push_back(0);
						if (policy.sparks) expected.push_back(1);
						if (policy.particles) expected.push_back(2);
						if (policy.lightning) expected.push_back(3);
						expected.push_back(10 + window);
						if (policy.postprocess) expected.push_back(20);
						trace.clear();
						RenderEffects(window);
						if (trace != expected) throw std::runtime_error("Camera effects selection/order differs from actual RenderEffects");
						++cases;
						camera_cases += input.camera;
					}
	for (unsigned mask = 0; mask < 32; ++mask)
		for (int type : { 2, 4, 6, 16 }) {
			extraGameInfo[0].bUseCameras = (mask & 1) != 0;
			IsMultiGame = (mask & 2) != 0;
			gameStates.app.bHaveExtraGameInfo[1] = (mask & 4) != 0;
			extraGameInfo[1].bUseCameras = (mask & 8) != 0;
			gameStates.render.cameras.bActive = (mask & 16) != 0;
			viewer.info.nType = type;
			source_monitor_gate();
			if (gameStates.render.bDoCameras != xl_compat::camera_monitors_enabled(extraGameInfo[0].bUseCameras, IsMultiGame,
			                                                                       gameStates.app.bHaveExtraGameInfo[1], extraGameInfo[1].bUseCameras, gameStates.render.cameras.bActive) ||
			    bool(source_automap_gate()) != xl_compat::camera_discovers_automap(gameStates.render.cameras.bActive, type))
				throw std::runtime_error("Camera automap/recursive-monitor policy differs from source");
			++visibility_cases;
		}
	std::cout << "Pinned camera view policy: " << cases << " RenderEffects traces (" << camera_cases << " camera views) and " << visibility_cases << " automap/monitor predicates passed\n";
}
