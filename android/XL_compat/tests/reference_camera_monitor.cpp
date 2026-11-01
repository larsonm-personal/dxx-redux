#include "camera_monitor.h"
#include <array>
#include <cstdint>
#include <iostream>
namespace camera_monitor_source
{
#define DBG            0
#define RENDER2TEXTURE 2
#define EGI_FLAG(...)  false
struct CBitmap {
};
struct CSegFace {
	struct {
		int nCamera = 0, nIndex = 0, nOvlTex = 0, bTeleport = 0;
		int index[4]{};
	} m_info;
	CBitmap *bmBot = nullptr, *bmTop = nullptr;
	float *pTexCoord = nullptr;
};
struct {
	float texCoord[16]{}, vertices[16]{};
} faces;
#define FACES faces
struct CCamera : CBitmap {
	bool valid = false, texture = false, visible = false;
	int buffer = 0, aligns = 0;
	std::array<float, 6> uv{};
	int GetTeleport()
	{
		return 0;
	}
	int HaveBuffer(int check)
	{
		if (check != 1) throw std::runtime_error("Wrong source monitor availability query");
		return buffer;
	}
	bool Valid()
	{
		return valid;
	}
	CBitmap *Texture()
	{
		return texture ? this : nullptr;
	}
	void Align(CSegFace *face, void *fixed, float *coords, float *vertices)
	{
		if (fixed || coords != faces.texCoord + face->m_info.nIndex || vertices != faces.vertices + face->m_info.nIndex)
			throw std::runtime_error("Unexpected source monitor alignment inputs");
		++aligns;
	}
	float *TexCoord()
	{
		return uv.data();
	}
	void SetVisible(int value)
	{
		visible = value != 0;
	}
};
struct {
	CCamera *camera = nullptr;
	std::array<int, 12> bindings{};
	CCamera *operator[](int index)
	{
		if (index != 3) throw std::runtime_error("Wrong source camera index");
		return camera;
	}
	int GetFaceCamera(int face)
	{
		return bindings.at(face);
	}
} cameraManager;
struct {
	struct {
		bool bDoCameras = false;
	} render;
} gameStates;
struct Color {
	float alpha = 1;
	float &Alpha()
	{
		return alpha;
	}
};
struct {
	struct {
		struct {
			Color vertices[4];
		} color;
	} renderData;
} gameData;
struct Options {
	struct {
		struct {
			bool bFitToWall = false;
		} cameras;
	} render;
} options;
Options *gameOpts = &options;
#include "xl_reference_camera_monitor.h"
#undef FACES
#undef EGI_FLAG
#undef RENDER2TEXTURE
#undef DBG
} // namespace camera_monitor_source
void camera_monitor_source_controls()
{
	using namespace camera_monitor_source;
	unsigned cases = 0, aligned = 0, overlays = 0, retained = 0, lookups = 0;
	CBitmap base, overlay;
	std::array<float, 6> authored_uv{};
	for (unsigned mask = 0; mask < 512; ++mask)
		for (int buffer : { -1, 0, 1, 2 })
			for (int overlay_texture : { 0, 17, 4095 }) {
				CCamera camera;
				camera.valid = (mask & 4) != 0;
				camera.texture = (mask & 8) != 0;
				camera.buffer = buffer;
				camera.visible = (mask & 128) != 0;
				cameraManager.camera = (mask & 1) ? &camera : nullptr;
				gameStates.render.bDoCameras = (mask & 2) != 0;
				options.render.cameras.bFitToWall = (mask & 64) != 0;
				CSegFace expected;
				expected.m_info.nCamera = 3;
				expected.m_info.nIndex = 4;
				expected.m_info.nOvlTex = overlay_texture;
				expected.m_info.bTeleport = (mask & 256) != 0;
				expected.bmBot = (mask & 16) ? &base : nullptr;
				expected.bmTop = (mask & 32) ? &overlay : nullptr;
				expected.pTexCoord = authored_uv.data();
				auto actual = expected;
				const xl_compat::CameraMonitorInput input{ cameraManager.camera != nullptr, gameStates.render.bDoCameras,
					                                       camera.valid, camera.texture, buffer, actual.bmBot != nullptr, overlay_texture, options.render.cameras.bFitToWall, false };
				const auto selection = xl_compat::camera_monitor_selection(input);
				bool visible = camera.visible;
				if (selection.clear_camera) actual.m_info.nCamera = -1;
				if (selection.visible) {
					visible = true;
					actual.m_info.bTeleport = 0;
				}
				if (selection.align) actual.pTexCoord = camera.uv.data();
				if (selection.image == xl_compat::CameraMonitorImage::base) actual.bmBot = &camera;
				if (selection.image == xl_compat::CameraMonitorImage::overlay) actual.bmTop = &camera;
				const bool handled = SetupMonitorFace(1, 2, 3, &expected) != 0;
				if (selection.handled != handled || expected.bmBot != actual.bmBot || expected.bmTop != actual.bmTop ||
				    expected.pTexCoord != actual.pTexCoord || expected.m_info.nCamera != actual.m_info.nCamera ||
				    expected.m_info.bTeleport != actual.m_info.bTeleport || visible != camera.visible || camera.aligns != int(selection.align))
					throw std::runtime_error("Ordinary camera monitor selection differs from actual source");
				++cases;
				aligned += selection.align;
				overlays += selection.image == xl_compat::CameraMonitorImage::overlay;
				retained += selection.image == xl_compat::CameraMonitorImage::unchanged;
			}
	for (unsigned i = 0; i < cameraManager.bindings.size(); ++i) cameraManager.bindings[i] = int(i) - 3;
	for (bool enabled : { false, true })
		for (bool force : { false, true })
			for (int segment = 0; segment < 2; ++segment)
				for (int side = 0; side < 6; ++side) {
					gameStates.render.bDoCameras = enabled;
					const int expected = enabled || force ? cameraManager.bindings[segment * 6 + side] : -1;
					if (IsMonitorFace(segment, side, force) != expected) throw std::runtime_error("Source monitor lookup gate differs");
					++lookups;
				}
	bool rejected = false;
	try {
		xl_compat::CameraMonitorInput input;
		input.exists = input.teleport = true;
		xl_compat::camera_monitor_selection(input);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	if (!rejected) throw std::runtime_error("Unqualified teleporter monitor was accepted");
	std::cout << "Pinned ordinary monitor selection: " << cases << " comparisons, " << aligned << " aligned, " << overlays << " overlay replacements, " << retained << " retained images and " << lookups << " source lookup controls passed\n";
}
