namespace addon_animation_source
{
using CTimeout = shield_pulse_source::CTimeout;
struct CTexture {
};
template <class T>
struct CStack {
};
struct CBitmap {
	struct {
		struct {
			int32_t nCurrent = 0;
			uint32_t nCount = 0;
			CBitmap *pBm = nullptr, *pCurrent = nullptr;
		} frames;
	} m_info;
	unsigned calls = 0;
	CTexture *Texture()
	{
		return nullptr;
	}
	CBitmap *CurFrame()
	{
		return m_info.frames.pCurrent;
	}
	CBitmap *AdvanceFrame();
	CBitmap *NextFrame()
	{
		++calls;
		return AdvanceFrame();
	}
};
struct CTGA {
	CBitmap *m_pBm = nullptr;
};
#define FILENAME_LEN 256
#include "xl_reference_addon_animation.h"
#undef FILENAME_LEN
// Data-only adapters own no bitmap allocation or GL resources
void CAddonBitmap::Unload()
{
	m_pBm = nullptr;
}
} // namespace addon_animation_source

static void compare_addon_animation()
{
	namespace source = addon_animation_source;
	shield_pulse_source::ticks = 1000;
	std::array<source::CAddonBitmap, 4> addons;
	std::array<source::CBitmap, 4> parents;
	std::array<std::vector<source::CBitmap>, 4> frames;
	std::array<xl_compat::AddonAnimationClock, 4> clocks;
	std::array<unsigned, 4> counts{ 1, 8, 17, 128 }, current{};
	for (unsigned i = 0; i < addons.size(); ++i) {
		frames[i].resize(counts[i]);
		parents[i].m_info.frames.nCount = counts[i];
		parents[i].m_info.frames.pBm = frames[i].data();
	}
	unsigned comparisons = 0, advances = 0, wraps = 0, multiple = 0, missing = 0, reloads = 0;
	uint32_t ticks = 2000;
	const auto compare = [&](unsigned index, int fps, bool bitmap, bool prepared) {
		auto &parent = parents[index];
		addons[index].m_pBm = bitmap ? &parent : nullptr;
		parent.m_info.frames.pCurrent = prepared ? frames[index].data() + parent.m_info.frames.nCurrent : nullptr;
		shield_pulse_source::ticks = ticks;
		const unsigned calls = parent.calls;
		addons[index].Animate(fps);
		const bool advance = clocks[index].advance(ticks, bitmap, prepared, fps);
		require(parent.calls - calls == unsigned(advance), "Addon animation expiry differs from source");
		if (advance) {
			current[index] = xl_compat::addon_next_frame(current[index], counts[index]);
			++advances;
			wraps += current[index] == 0;
		}
		require(parent.m_info.frames.nCurrent == int(current[index]), "Addon frame index differs from source");
		if (prepared) require(parent.m_info.frames.pCurrent == frames[index].data() + current[index], "Addon frame pointer differs from source");
		missing += !bitmap || !prepared;
		++comparisons;
		return advance;
	};
	for (unsigned event = 0; event < 4000; ++event) {
		ticks += std::array<uint32_t, 9>{ 0, 1, 49, 50, 99, 100, 101, 1000, 5000 }[event % 9];
		unsigned in_frame = 0;
		for (unsigned i = 0; i < addons.size(); ++i) {
			if (event && event % 211 == 0) {
				current[i] = 0;
				parents[i].m_info.frames.nCurrent = 0;
				++reloads;
			}
			const int fps = std::array<int, 4>{ 0, 10, 20, 1000 }[(event / 83 + i) % 4];
			const bool bitmap = (event + i) % 13 != 0, prepared = (event + i) % 17 != 0;
			in_frame += compare(i, fps, bitmap, prepared);
			require(!compare(i, fps, bitmap, prepared), "Repeated draw advanced one addon twice at the same tick");
		}
		multiple += in_frame > 1;
	}
	for (uint32_t now : { UINT32_MAX - 200, UINT32_MAX - 100, UINT32_MAX, uint32_t(9), uint32_t(109) }) {
		ticks = now;
		for (unsigned i = 0; i < addons.size(); ++i) compare(i, 10, true, true);
	}
	for (unsigned i = 0; i < addons.size(); ++i) {
		compare(i, 0, true, true);
		compare(i, 10, true, true);
	}
	ticks += 100;
	for (unsigned i = 0; i < addons.size(); ++i) require(compare(i, 10, true, true), "Restarted addon clock failed after tick wrap");
	require(advances && wraps && multiple && missing && reloads, "Addon animation missed lifecycle witnesses");
	std::cout << "Pinned addon animation: " << comparisons << " comparisons, " << advances << " advances, " << wraps
	          << " wraps and " << reloads << " bitmap reloads; independent clocks, missing frames and SDL wrap passed\n";
}
