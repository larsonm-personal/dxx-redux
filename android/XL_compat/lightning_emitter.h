#ifndef DXX_XL_LIGHTNING_EMITTER_H
#define DXX_XL_LIGHTNING_EMITTER_H
#include "lightning_bolt.h"
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct LightningEmitter {
	std::vector<LightningBolt> bolts;
	bool enabled;
	int updated = -1;
};

template <class Random>
LightningEmitter create_lightning_emitter(const LightningDefinition &definition, const LightningPoint &position, Random &random, int segment = -1)
{
	if (definition.bolts <= 0 || definition.bolts > 32767)
		throw std::runtime_error("XL lightning emitter has an invalid bolt count");
	LightningEmitter emitter{ {}, definition.enabled };
	emitter.bolts.reserve(definition.bolts);
	for (int i = 0; i < definition.bolts; ++i) {
		emitter.bolts.push_back(create_lightning_bolt(definition, position, random));
		emitter.bolts.back().segment = segment;
	}
	return emitter;
}

inline void lightning_emitter_move(LightningEmitter &emitter, const LightningPoint &position, int segment)
{
	for (auto &bolt : emitter.bolts) lightning_bolt_move(bolt, position, segment);
}

template <class Random, class RenderSetup>
bool lightning_emitter_frame(LightningEmitter &emitter, int time, int tick_time, Random &random, RenderSetup render_setup)
{
	if (time < 0 || time < emitter.updated || tick_time < 0)
		throw std::runtime_error("XL lightning emitter received an invalid clock");
	if (std::int64_t(time) - emitter.updated < 25) return false;
	emitter.updated = time;
	// Source animates every bolt before evaluating any lifetime transition
	if (emitter.enabled)
		for (auto &bolt : emitter.bolts) {
			lightning_bolt_animate(bolt, tick_time, random);
			if (bolt.lifetime.nodes > 0) render_setup(bolt);
		}
	for (auto &bolt : emitter.bolts)
		if (!lightning_bolt_set_life(bolt, random))
			throw std::runtime_error("Authored persistent lightning unexpectedly expired");
	return true;
}
template <class Random>
bool lightning_emitter_frame(LightningEmitter &emitter, int time, int tick_time, Random &random)
{
	return lightning_emitter_frame(emitter, time, tick_time, random, [](const LightningBolt &) {});
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
