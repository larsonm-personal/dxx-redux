#include <cmath>
#include <cstdio>
#include <string>
namespace camera_capture_source
{
struct CObject {
	int id;
} original{ 7 }, attached{ 9 }, changed{ 91 };
struct World {
	struct {
		struct {
			struct {
				int bActive = 0;
			} cameras;
			struct {
				int nType = 0;
			} cockpit;
		} render;
	} state;
	CObject *viewer = nullptr, *observed = nullptr;
	bool release = true, disable = true, canvas = false, mutate = false;
	int create_result = 1, create_calls = 0, setup_calls = 0, bpp = 0;
	std::array<int, 4> spec{};
	std::vector<int> trace;
} world;
auto &gameStates = world.state;
struct CCanvas {
	int width = 0, height = 0;
	int Width() const
	{
		return width;
	}
	int Height() const
	{
		return height;
	}
	void SetWidth(int value)
	{
		width = value;
	}
	void SetHeight(int value)
	{
		height = value;
	}
};
struct {
	struct {
		CCanvas screen;
	} renderData;
	CObject *SetViewer(CObject *viewer)
	{
		world.trace.push_back(1000 + (viewer ? viewer->id : -1));
		auto *saved = world.viewer;
		world.viewer = viewer;
		return saved;
	}
} gameData;
struct {
	struct {
		struct {
			int bHires = 0;
		} cameras;
	} render;
} options;
auto *gameOpts = &options;
struct Cockpit {
	void Activate(int type)
	{
		world.trace.push_back(200 + type);
		gameStates.render.cockpit.nType = type;
	}
} cockpit_value;
auto *cockpit = &cockpit_value;
void RenderFrame(int stereo, int flip)
{
	if (stereo || flip || !gameStates.render.cameras.bActive || !world.canvas || !world.viewer)
		throw std::runtime_error("Source camera entered its frame with invalid state");
	world.trace.push_back(400);
	world.observed = world.viewer;
	if (world.mutate) {
		world.viewer = &changed;
		gameStates.render.cockpit.nType = 5;
	}
}
int Pow2ize(int)
{
	throw std::runtime_error("Shadow camera sizing is outside the ordinary capture control");
}
struct FBO {
	int Create(int width, int height, int type, int colors)
	{
		world.spec = { width, height, type, colors };
		++world.create_calls;
		return world.create_result;
	}
	unsigned DepthBuffer()
	{
		throw std::runtime_error("Unexpected shadow camera depth buffer");
	}
	unsigned ColorBuffer()
	{
		return 42;
	}
};
struct Host {
	void camera_active(bool value)
	{
		gameStates.render.cameras.bActive = value;
	}
	int cockpit_type()
	{
		return gameStates.render.cockpit.nType;
	}
	void activate_cockpit(int type)
	{
		cockpit->Activate(type);
	}
	int release_buffer()
	{
		world.trace.push_back(302);
		return world.release;
	}
	CObject *set_viewer(CObject *viewer)
	{
		return gameData.SetViewer(viewer);
	}
	void activate_canvas()
	{
		world.trace.push_back(300);
		world.canvas = true;
	}
	void render_frame()
	{
		RenderFrame(0, 0);
	}
	void deactivate_canvas()
	{
		world.trace.push_back(301);
		world.canvas = false;
	}
	int disable_buffer()
	{
		world.trace.push_back(303);
		return world.disable;
	}
};
struct CCamera : CCanvas {
	struct {
		FBO fbo;
		int nId, bShadowMap, bVisible, bValid;
		unsigned glTexId;
		CObject *pObj;
		CObject obj;
	} m_data{};
	std::string name;
	void SetName(const char *value)
	{
		name = value;
	}
	void SetBPP(int value)
	{
		world.bpp = value;
	}
	void Setup(int, int16_t, int16_t, int16_t, int16_t, CObject *object, int)
	{
		++world.setup_calls;
		m_data.pObj = object;
		m_data.obj.id = 19;
	}
	int ReleaseBuffer()
	{
		return Host{}.release_buffer();
	}
	void Activate(const char *)
	{
		Host{}.activate_canvas();
	}
	void Deactivate()
	{
		Host{}.deactivate_canvas();
	}
	int DisableBuffer()
	{
		return Host{}.disable_buffer();
	}
	void Init();
	int CreateBuffer();
	int Create(int16_t, int16_t, int16_t, int16_t, int16_t, CObject *, int32_t, int32_t);
	int Render();
};
#include "xl_reference_camera_capture.h"
static_assert(CM_FULL_SCREEN == xl_compat::camera_full_screen, "Camera cockpit constant differs");
#undef CM_FULL_SCREEN
} // namespace camera_capture_source

void camera_capture_source_controls()
{
	using namespace camera_capture_source;
	const auto require = [](bool condition, const char *message) { if (!condition) throw std::runtime_error(message); };
	unsigned sizes = 0, transactions = 0, retained = 0, successful = 0;
	for (int width : { 2, 3, 5, 127, 128, 129, 640, 641, 1920, 32767 })
		for (int height : { 2, 3, 5, 127, 128, 129, 480, 481, 1080, 32767 })
			for (bool hires : { false, true })
				for (int result : { -1, 0, 1 }) {
					world = {};
					world.create_result = result;
					gameData.renderData.screen = { width, height };
					options.render.cameras.bHires = hires;
					CCamera source;
					const auto spec = xl_compat::camera_target_spec(width, height, hires);
					const int created = source.Create(7, 1, 2, 3, 4, &attached, 0, 0);
					require(world.spec == std::array<int, 4>{ spec.width, spec.height, spec.buffer_type, spec.color_buffers }, "Camera target specification differs from source");
					require(source.Width() == spec.width && source.Height() == spec.height && world.bpp == 4 && world.create_calls == 1,
					        "Camera canvas creation differs from source");
					require(created == (result != 0) && world.setup_calls == (result != 0), "Source camera creation result changed");
					require(source.m_data.glTexId == (result ? 42u : 0u), "Source camera published a different buffer handle");
					++sizes;
				}
	for (unsigned mode = 0; mode < 64; ++mode)
		for (int cockpit_type : { 0, 1, 2, 3, 4, 5 })
			for (CObject *viewer : { static_cast<CObject *>(nullptr), &original, &changed }) {
				CCamera source;
				source.m_data.bValid = bool(mode & 4);
				source.m_data.pObj = mode & 8 ? &attached : nullptr;
				source.m_data.obj.id = 19;
				xl_compat::CameraCapture portable{ bool(mode & 4) };
				for (unsigned step = 0; step < 8; ++step) {
					World initial;
					initial.state.render.cameras.bActive = bool(mode & 16);
					initial.state.render.cockpit.nType = cockpit_type;
					initial.viewer = viewer;
					initial.release = bool(mode & 1) != (step % 3 == 0);
					initial.disable = bool(mode & 2);
					initial.mutate = bool(mode & 32);
					world = initial;
					const auto expected = source.Render();
					const auto snapshot = world;
					world = initial;
					Host host;
					const auto actual = portable.render(host, source.m_data.pObj, &source.m_data.obj);
					require(actual == bool(expected) && portable.valid == bool(source.m_data.bValid), "Camera capture validity differs from source");
					require(world.trace == snapshot.trace && world.state.render.cameras.bActive == snapshot.state.render.cameras.bActive &&
					            world.state.render.cockpit.nType == snapshot.state.render.cockpit.nType && world.viewer == snapshot.viewer &&
					            world.observed == snapshot.observed && world.canvas == snapshot.canvas,
					        "Camera capture transaction differs from source");
					successful += initial.release;
					retained += !initial.release && actual;
					++transactions;
				}
			}
	unsigned rejected = 0;
	for (const auto &size : { std::array<int, 2>{ 0, 480 }, { 640, 0 }, { -1, 480 }, { 32768, 480 }, { 640, 32768 }, { 1, 480 }, { 640, 1 } }) {
		try {
			(void) xl_compat::camera_target_spec(size[0], size[1], false);
		} catch (const std::runtime_error &) {
			++rejected;
		}
	}
	require(rejected == 7 && retained && successful, "Camera capture controls missed coverage");
	std::cout << "Pinned ordinary camera capture: " << sizes << " target specifications, " << transactions << " transactions, " << successful
	          << " successful captures, " << retained << " retained-valid buffer misses and " << rejected << " rejected dimensions passed\n";
}
