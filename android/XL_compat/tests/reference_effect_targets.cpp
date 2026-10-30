// Source target normalization with storage adapters, before activation consumers
#include "effect_targets.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace effect_target_reference
{
#define MAX_TRIGGER_TARGETS 10
#define PrintLog(...)
struct CTriggerTargets {
	int m_nLinks = 0;
	int16_t m_segments[10]{}, m_sides[10]{};
	void Check();
};
struct CTrigger : CTriggerTargets {
};
struct {
	CTrigger item;
	CTrigger *Buffer()
	{
		return &item;
	}
} GEOTRIGGERS;
struct {
	struct {
		int nSegments = 3;
	} segData;
	struct {
		int nObjects = 52;
	} objData;
} gameData;
struct {
	struct {
		int count = 1;
	} triggers;
} gameFileInfo;
#include "xl_reference_effect_targets.h"
} // namespace effect_target_reference
void effect_target_source_controls()
{
	using namespace effect_target_reference;
	unsigned cases = 0;
	std::uint32_t random = 0x430157u;
	for (int type : { 32, 33 })
		for (bool attached : { false, true })
			for (unsigned variant = 0; variant < 1024; ++variant) {
				xl_compat::Trigger action{ type, 0, 0, -1, {} };
				const int indices[]{ -32768, -1, 0, 1, 2, 3, 49, 50, 51, 52, 32767 };
				const int sides[]{ -32768, -2, -1, 0, 1, 5, 6, 32767 };
				auto &reference = GEOTRIGGERS.item;
				reference = {};
				reference.m_nLinks = int(variant % 11);
				for (int i = 0; i < reference.m_nLinks; ++i) {
					random = random * 1664525u + 1013904223u;
					const int index = indices[(random >> 8) % 11];
					random = random * 1664525u + 1013904223u;
					const int side = sides[(random >> 8) % 8];
					action.targets.push_back({ index, side });
					reference.m_segments[i] = int16_t(index);
					reference.m_sides[i] = int16_t(side);
				}
				reference.Check();
				if (!attached) CheckTriggerInfo();
				std::vector<std::array<int, 2>> expected;
				for (int i = 0; i < reference.m_nLinks; ++i) expected.push_back({ reference.m_segments[i], reference.m_sides[i] });
				if (xl_compat::effect_targets(action, 3, 52, attached) != expected)
					throw std::runtime_error("Effect target normalization differs from actual source");
				++cases;
			}
	std::cout << "Pinned XL effect targets: " << cases << " wall/attached ordered lists match actual reader and geometry cleanup\n";
}
