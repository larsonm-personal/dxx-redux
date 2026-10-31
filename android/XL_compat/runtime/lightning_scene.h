#ifndef DXX_XL_RUNTIME_LIGHTNING_SCENE_H
#define DXX_XL_RUNTIME_LIGHTNING_SCENE_H
#include "../lightning_render.h"
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct LightningRenderFrame {
	LightningCore core;
	LightningGlow glow;
};
struct LightningSceneItem {
	LightningRenderFrame frame;
	LightningPoint position;
	std::array<float, 4> color;
	int object, signature, random_nodes;
	float width;
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
