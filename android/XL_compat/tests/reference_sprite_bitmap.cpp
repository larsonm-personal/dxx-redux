#include "sprite_render.h"
#include <iostream>
#include <vector>
namespace sprite_bitmap_source
{
constexpr int TRANSPARENCY_COLOR = 255, SUPER_TRANSP_COLOR = 254;
struct CRGBColor {
	uint8_t rgb[3]{};
	uint8_t Red() const
	{
		return rgb[0];
	}
	uint8_t Green() const
	{
		return rgb[1];
	}
	uint8_t Blue() const
	{
		return rgb[2];
	}
};
struct CComputedColor : CRGBColor {
	int nIndex = 0;
};
struct CPalette {
	struct {
		CRGBColor rgb[256];
	} m_data;
	int m_nComputedColors = 0;
	CComputedColor m_computedColors[1];
	void AddComputedColor(int, int, int, int) {}
	CRGBColor *Color()
	{
		return m_data.rgb;
	}
	int32_t ClosestColor(int32_t, int32_t, int32_t);
};
struct CFloatVector {
	float values[4]{};
	float &Red()
	{
		return values[0];
	}
	float &Green()
	{
		return values[1];
	}
	float &Blue()
	{
		return values[2];
	}
	float &Alpha()
	{
		return values[3];
	}
};
struct CBitmap {
	struct {
		CPalette *palette;
		int avgColorIndex = 0;
		struct {
			int w = 0, h = 0;
		} props;
		int nBPP = 1;
		CRGBColor avgColor;
	} m_info;
	std::vector<uint8_t> pixels;
	uint8_t *Buffer()
	{
		return pixels.data();
	}
	int32_t AvgColorIndex();
	CFloatVector *GetAvgColor(CFloatVector *);
};
int Sqr(int value)
{
	return value * value;
}
#include "xl_reference_sprite_bitmap.h"
} // namespace sprite_bitmap_source
void sprite_bitmap_controls()
{
	using namespace sprite_bitmap_source;
	unsigned cases = 0;
	for (unsigned seed = 0; seed < 16; ++seed) {
		CPalette palette;
		std::array<unsigned char, 768> native_palette{};
		for (unsigned i = 0; i < 256; ++i)
			for (unsigned axis = 0; axis < 3; ++axis)
				palette.m_data.rgb[i].rgb[axis] = native_palette[i * 3 + axis] = uint8_t((i * (axis * 6 + 1) + seed * 13) % 64);
		for (unsigned average : { 0u, 1u, 63u, 254u, 255u })
			for (unsigned profile = 0; profile < 4; ++profile) {
				CBitmap bitmap;
				bitmap.m_info.palette = &palette;
				bitmap.m_info.avgColorIndex = int(average);
				bitmap.m_info.props = { 7, 5 };
				bitmap.pixels.resize(35);
				std::array<unsigned char, 45> strided{};
				for (unsigned i = 0; i < 35; ++i) {
					const auto pixel = uint8_t(profile == 0   ? (i + seed) % 3 == 0 ? 0 : (i + seed) % 3 == 1 ? 254
					                                                                                          : 255
					                           : profile == 1 ? 1
					                                          : (i * 11 + seed * 29) % 256);
					bitmap.pixels[i] = strided[(i / 7) * 9 + i % 7] = pixel;
				}
				CFloatVector source;
				bitmap.GetAvgColor(&source);
				if (source.Red() + source.Green() + source.Blue() == 0) source.Red() = source.Green() = source.Blue() = 1;
				const auto native = xl_compat::sprite_bitmap_color(average, strided.data(), 7, 5, 9, native_palette.data());
				for (unsigned axis = 0; axis < 3; ++axis)
					if (native[axis] != source.values[axis]) throw std::runtime_error("Sprite bitmap color differs from pinned source");
				++cases;
			}
	}
	std::cout << "Source sprite bitmap colors: " << cases << " stored-index/decoded-average/palette/stride controls passed\n";
}
