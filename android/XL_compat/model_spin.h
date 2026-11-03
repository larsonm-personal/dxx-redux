#ifndef DXX_XL_MODEL_SPIN_H
#define DXX_XL_MODEL_SPIN_H
#include "oof_mesh.h"
namespace xl_compat
{
// Source G3AnimateSubModel: 32 frames, five clockwise revolutions about local Z
inline void spin_model_vertex(OofRenderVertex &vertex, const OofVector &center, unsigned frame)
{
	const float angle = -6.2831853071795864769f * 5 * float(frame % 32) / 32;
	const float c = std::cos(angle), s = std::sin(angle);
	const float x = vertex.position[0] - center[0], y = vertex.position[1] - center[1];
	vertex.position[0] = center[0] + c * x - s * y;
	vertex.position[1] = center[1] + s * x + c * y;
	const float nx = vertex.normal[0], ny = vertex.normal[1];
	vertex.normal[0] = c * nx - s * ny;
	vertex.normal[1] = s * nx + c * ny;
}
} // namespace xl_compat
#endif
