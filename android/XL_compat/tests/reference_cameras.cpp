#include "camera_schedule.h"
#include "camera_alignment.h"
#include "camera_capture.h"
#include <array>
#include <cstring>
#include <iostream>
#include "reference_camera_capture.h"

namespace camera_reference
{
using time_t = std::int64_t;
using fix = std::int32_t;
#define DBG    0
#define EPS    0.01f
#define F2X(v) fix((v) * 65536)
#define X2F(v) (float(v) / 65536)
#define I2X(v) ((v) * 65536)
constexpr int SIDE_IS_TRI_13 = 3;
struct tUVL {
	fix u, v, l;
};
struct tTexCoord2f {
	xl_compat::CameraUV v;
};
struct CFloatVector3 {
};
struct CFloatVector {
	static float Dist(const CFloatVector &, const CFloatVector &)
	{
		throw std::runtime_error("Source camera used disabled fit-to-wall path");
	}
};
struct CSegFace {
	struct {
		int nType = 0;
	} m_info;
};
struct CCanvas {
	int width = 640, height = 480;
	int Width() const
	{
		return width;
	}
	int Height() const
	{
		return height;
	}
	static CCanvas *Current()
	{
		static CCanvas canvas;
		return &canvas;
	}
};
struct CObject {
	struct {
		unsigned nFlags = 0;
	} info;
};
struct {
	struct {
		struct {
			int nFPS = 0;
			int bHires = 0;
		} cameras;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		bool bD2XLevel = true;
		bool bHaveExtraGameInfo[2]{ true, true };
		time_t nSDLTicks[1]{};
	} app;
	struct {
		bool bTriangleMesh = false;
		struct {
			int nType = 0;
		} cockpit;
	} render;
} gameStates;
struct {
	bool bUseCameras = true;
} extraGameInfo[2];
bool IsMultiGame = false, teleporter_enabled = false;
#define EGI_FLAG(a, b, c, d) teleporter_enabled
time_t ticks = 0;
time_t SDL_GetTicks()
{
	return ticks;
}
struct {
	struct {
		CObject *pViewer = nullptr;
	} objData;
	CObject *SetViewer(CObject *object)
	{
		auto *old = objData.pViewer;
		objData.pViewer = object;
		return old;
	}
} gameData;
struct Cockpit {
	void Activate(int type)
	{
		gameStates.render.cockpit.nType = type;
	}
} cockpit_value;
auto *cockpit = &cockpit_value;
int rendered = -1;
struct CCamera {
	struct {
		int nWaitFrames = 0;
		bool bVisible = false, bTeleport = false, bTimedOut = false;
		time_t nTimeout = 0;
		CObject *pObj = nullptr;
		bool bAligned = false, bMirror = false;
		tUVL uvlList[4]{};
		tTexCoord2f texCoord[6]{};
	} m_data;
	bool buffer = true;
	int width = 1024, height = 512;
	int Width() const
	{
		return width;
	}
	int Height() const
	{
		return height;
	}
	int HaveBuffer(int) const
	{
		return buffer ? 1 : 0;
	}
	void Align(CSegFace *, tUVL *, tTexCoord2f *, CFloatVector3 *);
	int type = 0, id = 0;
	int Type() const
	{
		return type;
	}
	int Ready(time_t t);
	void Reset();
	int Render()
	{
		if (rendered != -1) throw std::runtime_error("Source selected multiple cameras in one frame");
		rendered = id;
		gameData.SetViewer(m_data.pObj);
		cockpit->Activate(2);
		return 1;
	}
};
struct CCameraManager {
	struct {
		std::vector<CCamera> entries;
		unsigned ToS() const
		{
			return unsigned(entries.size());
		}
		CCamera &operator[](unsigned i)
		{
			return entries.at(i);
		}
		CCamera *operator+(unsigned i)
		{
			return &entries.at(i);
		}
	} m_cameras;
	CCamera *m_current = nullptr;
	int Render();
};
#include "xl_reference_cameras.h"

void same(const xl_compat::CameraSchedule &portable, const CCamera &source)
{
	const auto &state = source.m_data;
	if (portable.wait_frames != state.nWaitFrames || portable.visible != state.bVisible || portable.teleport != state.bTeleport ||
	    portable.timed_out != state.bTimedOut || portable.timeout != state.nTimeout)
		throw std::runtime_error("Camera scheduler state differs from pinned source");
}
} // namespace camera_reference

void camera_schedule_source_controls()
{
	using namespace camera_reference;
	unsigned comparisons = 0, selections = 0, frames = 0;
	for (int fps : { 0, 1, 5, 30, 60, 90, 1000 })
		for (int flags = 0; flags < 16; ++flags)
			for (int mode = 0; mode < 32; ++mode)
				for (int delta : { -1, 0, 1, 10, 11, 16, 17, 33, 34, 999, 1000 }) {
					CObject owner{ { unsigned(flags) } };
					CCamera source;
					source.m_data.nWaitFrames = 19;
					source.m_data.bVisible = bool(mode & 1);
					source.m_data.bTeleport = bool(mode & 2);
					source.m_data.bTimedOut = bool(mode & 4);
					source.m_data.nTimeout = 1000;
					source.m_data.pObj = mode & 8 ? &owner : nullptr;
					xl_compat::CameraSchedule portable{ 19, 1000, bool(mode & 1), bool(mode & 4), bool(mode & 2) };
					gameOpts->render.cameras.nFPS = fps;
					teleporter_enabled = (mode & 16) != 0;
					const auto ready = portable.ready(1000 + delta, fps, teleporter_enabled, mode & 8 ? unsigned(flags) : 0);
					if (ready != source.Ready(1000 + delta)) throw std::runtime_error("Camera readiness differs from pinned source");
					same(portable, source);
					gameStates.app.nSDLTicks[0] = 2345;
					portable.reset(2345);
					source.Reset();
					same(portable, source);
					++comparisons;
				}
	for (int fps : { 0, 1, 30, 60, 1000 })
		for (unsigned count : { 0u, 1u, 3u, 8u }) {
			CCameraManager source;
			source.m_cameras.entries.resize(count);
			std::vector<xl_compat::ScheduledCamera> portable(count);
			std::array<CObject, 8> owners;
			CObject viewer;
			gameOpts->render.cameras.nFPS = fps;
			for (unsigned i = 0; i < count; ++i) {
				auto &camera = source.m_cameras.entries[i];
				camera.id = int(i);
				camera.m_data.pObj = &owners[i];
				camera.type = portable[i].type = i == 6 ? 3 : 0;
				camera.m_data.bTeleport = portable[i].schedule.teleport = i % 3 == 1;
			}
			for (unsigned frame = 0; frame < 1500; ++frame) {
				ticks = 1000 + frame * 7;
				gameStates.app.nSDLTicks[0] = ticks - frame % 3;
				gameStates.app.bD2XLevel = frame % 13 != 0;
				extraGameInfo[0].bUseCameras = frame % 17 != 0;
				extraGameInfo[1].bUseCameras = frame % 19 != 0;
				gameStates.app.bHaveExtraGameInfo[1] = frame % 23 != 0;
				IsMultiGame = frame % 5 == 0;
				teleporter_enabled = frame % 11 != 0;
				for (unsigned i = 0; i < count; ++i) {
					if ((frame + i * 3) % 7 == 0) source.m_cameras[i].m_data.bVisible = portable[i].schedule.visible = true;
					owners[i].info.nFlags = portable[i].owner_flags = (frame + i) % 29 == 0 ? 1u << ((frame + i) % 4) : 0;
				}
				gameData.objData.pViewer = &viewer;
				gameStates.render.cockpit.nType = int(frame % 4);
				rendered = -1;
				const bool enabled = gameStates.app.bD2XLevel && extraGameInfo[0].bUseCameras &&
				                     (!IsMultiGame || (gameStates.app.bHaveExtraGameInfo[1] && extraGameInfo[1].bUseCameras));
				const int selected = xl_compat::select_camera(portable, ticks, gameStates.app.nSDLTicks[0], fps, enabled, teleporter_enabled);
				source.Render();
				if (selected != rendered || gameData.objData.pViewer != &viewer || gameStates.render.cockpit.nType != int(frame % 4))
					throw std::runtime_error("Camera arbitration or source viewer/cockpit restoration differs");
				for (unsigned i = 0; i < count; ++i) same(portable[i].schedule, source.m_cameras[i]);
				selections += selected >= 0;
				++frames;
			}
		}
	if (!selections) throw std::runtime_error("Camera scheduling controls rendered no cameras");
	std::cout << "Pinned cameras: " << comparisons << " readiness/reset comparisons, " << frames << " manager frames and " << selections << " selected updates; visibility, timeout latch, death flags, stable ties and one-camera cadence passed\n";
	unsigned alignments = 0;
	std::uint32_t seed = 519;
	for (int mode = 0; mode < 48; ++mode) {
		CCamera source;
		xl_compat::CameraAlignment portable;
		gameStates.render.bTriangleMesh = mode % 3 != 0;
		CSegFace face{ { mode % 3 == 2 ? SIDE_IS_TRI_13 : 2 } };
		gameOpts->render.cameras.bHires = (mode / 3) & 1;
		source.buffer = (mode / 6) & 1;
		source.m_data.bMirror = (mode / 12) & 1;
		for (int sample = 0; sample < 2048; ++sample) {
			std::array<xl_compat::CameraUV, 6> input{};
			std::array<tTexCoord2f, 6> reference{};
			for (int i = 0; i < 6; ++i) {
				seed = seed * 1664525u + 1013904223u;
				input[i].u = float(int(seed % 32769) - 16384) / 4096;
				seed = seed * 1664525u + 1013904223u;
				input[i].v = float(int(seed % 32769) - 16384) / 4096;
			}
			// Exact and adjacent epsilon thresholds exercise each orientation branch
			if (sample % 8 < 5) {
				constexpr float threshold[]{ -.01001f, -.01f, 0, .01f, .01001f };
				input[0].u = 0;
				input[1].u = threshold[sample % 8];
			}
			for (int i = 0; i < 6; ++i) reference[i].v = input[i];
			CCanvas::Current()->width = 320 + sample % 7 * 73;
			CCanvas::Current()->height = 240 + sample % 11 * 31;
			if (sample % 4 != 0) {
				source.m_data.bAligned = false;
				portable.invalidate();
			}
			source.Align(&face, nullptr, reference.data(), nullptr);
			portable.align(input, gameStates.render.bTriangleMesh, mode % 3 == 2,
			               source.m_data.bMirror, source.buffer, gameOpts->render.cameras.bHires != 0,
			               CCanvas::Current()->width, CCanvas::Current()->height, source.width, source.height);
			// Source quad path leaves the unused final two entries uninitialized
			for (int i = 0; i < (gameStates.render.bTriangleMesh ? 6 : 4); ++i)
				if (portable.coordinates[i].u != source.m_data.texCoord[i].v.u || portable.coordinates[i].v != source.m_data.texCoord[i].v.v)
					throw std::runtime_error("Camera monitor UV alignment differs from pinned source");
			if (portable.aligned != source.m_data.bAligned) throw std::runtime_error("Camera alignment cache differs");
			++alignments;
		}
	}
	std::cout << "Pinned camera alignment: " << alignments << " float monitor UV comparisons; quad/both triangle diagonals, rotations, mirrors, buffer dimensions, epsilon thresholds and cached realignment passed\n";
}
