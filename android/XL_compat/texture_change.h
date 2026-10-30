#ifndef DXX_XL_TEXTURE_CHANGE_H
#define DXX_XL_TEXTURE_CHANGE_H
#include <cstdint>

namespace xl_compat
{
struct TextureChange {
	int base, overlay;
};
inline TextureChange texture_change(std::int32_t value)
{
	const auto packed = std::uint32_t(value);
	// CSide assigns a nonnegative high word to its 14-bit overlay field
	return { int(packed & 0xffff), value < 0 ? -1 : int((packed >> 16) & 0x3fff) };
}
inline int changed_overlay(int previous, int texture)
{
	return texture < 0 ? previous : (previous & 0xc000) | texture;
}
} // namespace xl_compat
#endif
