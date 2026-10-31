// Actual pinned Create face-selection body; Add records its material requests
namespace outdoors_material_reference
{
struct Face {
	struct {
		int nBaseTex, nOvlTex, nSide;
	} m_info;
};
struct Segment {
	bool outdoors;
	bool HasOutdoorsProp() const
	{
		return outdoors;
	}
};
struct Request {
	int texture, brightness;
	bool ambient;
};
struct Owner {
	xl_compat::TextureBrightnessTable brightness{};
	std::vector<Request> calls;
	int IsLight(int texture) const
	{
		return texture >= 0 && texture < 910 ? brightness[texture] : 0;
	}
	void Add(Face *, std::array<float, 3> *, int light, int, int, int, int texture, void *, bool ambient)
	{
		calls.push_back({ texture, light, ambient });
	}
	void select(int base, int overlay, bool outdoors)
	{
		Face face{ { base, overlay, 0 } };
		Segment segment{ outdoors };
		auto *pFace = &face;
		auto *pSeg = &segment;
		int nTexture = 0, nLight = 0, nSegment = 0, nSide = 0;
		std::array<float, 3> *pColor;
		struct {
			struct {
				struct {
					std::array<float, 3> textures[910];
				} color;
			} renderData;
		} gameData{};
		constexpr int MAX_WALL_TEXTURES = 910;
		calls.clear();
		for (int once = 0; once < 1; ++once) {
#include "xl_reference_outdoors_material.h"
		}
	}
};
inline void controls()
{
	Owner owner;
	owner.brightness[1] = 65536;
	owner.brightness[2] = 131072;
	owner.brightness[3] = -65536;
	xl_compat::TextureRgbTable colors{};
	for (auto &color : colors) color = { 1, 1, 1 };
	unsigned cases = 0;
	for (int base : { -1, 0, 1, 2, 3, 909, 910 })
		for (int overlay : { -1, 0, 1, 2, 3, 909, 910 })
			for (bool outdoors : { false, true }) {
				owner.select(base, overlay, outdoors);
				xl_compat::ClassicFaceLightMaterials materials(owner.brightness, colors);
				const auto actual = materials.face(base, overlay, outdoors);
				// Add rejects nonpositive brightness before Update; Update retains creation flags
				std::optional<Request> first, last;
				for (const auto &call : owner.calls)
					if (call.brightness > 0) {
						if (!first) first = call;
						last = call;
					}
				check(bool(actual) == bool(first), "Pinned face selection presence differs");
				if (actual) check(actual->created_texture == first->texture && actual->material_texture == last->texture && actual->ambient == first->ambient && actual->brightness == float(last->brightness) / 65536.0f, "Pinned outdoors face selection or ambient flag differs");
				++cases;
			}
	std::cout << cases << " pinned outdoors/ordinary face material selections passed\n";
}
} // namespace outdoors_material_reference
