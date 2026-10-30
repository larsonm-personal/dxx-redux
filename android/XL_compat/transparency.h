#ifndef DXX_XL_TRANSPARENCY_H
#define DXX_XL_TRANSPARENCY_H
#include "pog.h"
#include <optional>
namespace xl_compat
{
// Pinned CObject::InitTables, including robot and Vertigo energy variants
inline bool transparency_energy_projectile(unsigned id)
{
	switch (id) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 5:
		case 6:
		case 9:
		case 10:
		case 12:
		case 13:
		case 14:
		case 19:
		case 20:
		case 24:
		case 25:
		case 26:
		case 27:
		case 29:
		case 30:
		case 31:
		case 33:
		case 34:
		case 35:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		case 48:
		case 49:
		case 60:
		case 63:
		case 64: return true;
		default: return false;
	}
}
// Pinned FaceIsTransparent classifies alpha/keyed materials before queueing.
// A see-through cutout keeps opaque depth behavior; it is not a blended face
inline bool transparent_face(std::optional<unsigned> base, std::optional<unsigned> overlay, bool authored_transparent, bool additive, float alpha)
{
	if (!base) return authored_transparent || alpha < 1.0f;
	if (authored_transparent || additive) return true;
	if (*base & pog_see_through) return false;
	if (overlay && (*overlay & pog_super)) return true;
	if (!(*base & (pog_transparent | pog_super))) return false;
	if (!overlay) return true;
	if (*overlay & pog_see_through) return false;
	return (*overlay & pog_transparent) != 0;
}
} // namespace xl_compat
#endif
