// Execute the pinned texture operation with independent side and audio adapters
#include "../texture_change.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "xl_reference_trigger_flag_constants.h"

namespace texture_reference
{
struct CSide {
	std::int16_t m_nBaseTex = 0;
	std::uint16_t m_nOvlTex : 14;
	std::uint16_t m_nOvlOrient : 2;
	void SetTextures(int32_t, int32_t);
};
struct CSegment {
	std::array<CSide, 6> sides{};
	void SetTextures(int side, int base, int overlay)
	{
		sides.at(unsigned(side)).SetTextures(base, overlay);
	}
};
std::array<CSegment, 3> segments;
#define SEGMENT(index) (&segments.at(unsigned(index)))
struct {
	unsigned updates = 0;
	void Update()
	{
		++updates;
	}
} audio;
struct {
	struct {
		bool bD1Mission = false;
	} app;
} gameStates;
#define N_LOCALPLAYER 0
unsigned messages = 0;
void HUDInitMessage(const char *, const char *)
{
	++messages;
}
struct CTrigger {
	int value = 0, flags = 0, m_nLinks = 0;
	std::array<int, 10> m_segments{}, m_sides{};
	int GetValue() const
	{
		return value;
	}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	void DoChangeTexture();
	void PrintMessage(int32_t, int32_t, const char *);
};
#include "xl_reference_texture_actions.h"
#undef SEGMENT
#undef N_LOCALPLAYER
} // namespace texture_reference

void texture_action_source_controls()
{
	using namespace texture_reference;
	const auto require = [](bool value, const char *reason) { if (!value) throw std::runtime_error(reason); };
	unsigned cases = 0;
	for (unsigned base : { 0u, 1u, 434u, 909u })
		for (unsigned overlay : { 0u, 1u, 223u, 909u, 0x4000u, 0x4001u, 0x7fffu, 0x8000u, 0xffffu })
			for (unsigned orientation = 0; orientation < 4; ++orientation)
				for (int count : { 0, 1, 7, 10 }) {
					for (auto &segment : segments)
						for (auto &side : segment.sides) {
							side.m_nBaseTex = 117;
							side.m_nOvlTex = 221;
							side.m_nOvlOrient = orientation;
						}
					audio.updates = 0;
					CTrigger action;
					action.value = std::int32_t((overlay << 16) | base);
					action.m_nLinks = count;
					std::array<bool, 18> touched{};
					for (int i = 0; i < count; ++i) {
						action.m_segments[i] = i % 3;
						action.m_sides[i] = (i * 2) % 6;
						touched[unsigned(action.m_segments[i] * 6 + action.m_sides[i])] = true;
					}
					action.DoChangeTexture();
					const auto changed = xl_compat::texture_change(action.value);
					require(changed.base == int(base), "Texture base word differs from source");
					for (unsigned i = 0; i < touched.size(); ++i) {
						const auto &actual = segments[i / 6].sides[i % 6];
						const int expected_overlay = touched[i] ? xl_compat::changed_overlay(int(orientation << 14) | 221, changed.overlay) : int(orientation << 14) | 221;
						require(actual.m_nBaseTex == (touched[i] ? changed.base : 117) && actual.m_nOvlTex == (expected_overlay & 0x3fff) && actual.m_nOvlOrient == orientation,
						        "Pinned texture operation differs in target, overlay or retained orientation");
					}
					require(audio.updates == 1, "Texture operation did not invalidate audio routes exactly once");
					++cases;
				}
	for (int flags = 0; flags < 256; ++flags)
		for (int player : { -1, 0, 1 })
			for (bool d1 : { false, true }) {
				CTrigger action;
				action.flags = flags;
				gameStates.app.bD1Mission = d1;
				messages = 0;
				action.PrintMessage(player, 2, "Walls have been changed!");
				require(messages == unsigned(!d1 && player <= 0 && !(flags & TF_NO_MESSAGE)), "Texture message flag or player policy differs");
				++cases;
			}
	std::cout << "PASS: " << cases << " pinned texture action, orientation, target, route invalidation and message controls\n";
}
