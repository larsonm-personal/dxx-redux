#include "mission_intent_classification.hpp"

#include <cassert>
#include <cstring>

using dxx_mission_intent::Accumulator;
using dxx_mission_intent::Classification;
using dxx_mission_intent::LevelInputs;
using dxx_mission_intent::ModeDeclarations;

static LevelInputs level(int number, int starts, int robots = 0, int hostages = 0,
                         int matcens = 0, int guidebots = 0)
{
	LevelInputs result;
	result.level_num = number;
	result.player_starts = starts;
	result.robots = robots;
	result.hostages = hostages;
	result.matcens = matcens;
	result.guidebots = guidebots;
	return result;
}

int main()
{
	{
		Accumulator accumulator;
		accumulator.add(level(1, 8, 26));
		ModeDeclarations declarations;
		declarations.add("normal");
		declarations.add("coop");
		declarations.add("anarchy");
		const auto summary = accumulator.finish(declarations);
		assert(summary.classification == Classification::single_player_or_coop_and_multiplayer_anarchy);
		assert(!std::strcmp(summary.rule, "descriptor_both"));
	}
	{
		Accumulator accumulator;
		accumulator.add(level(1, 8));
		accumulator.add(level(2, 4));
		const auto summary = accumulator.finish(ModeDeclarations());
		assert(summary.classification == Classification::multiplayer_anarchy);
		assert(summary.arena_like_levels == 2);
	}
	{
		Accumulator accumulator;
		accumulator.add(level(1, 8, 1));
		const auto summary = accumulator.finish(ModeDeclarations());
		assert(summary.classification == Classification::single_player_or_coop);
		assert(summary.campaign_actor_levels == 1);
	}
	{
		Accumulator accumulator;
		accumulator.add(level(-1, 8, 10));
		accumulator.add(level(1, 1));
		const auto summary = accumulator.finish(ModeDeclarations());
		assert(summary.classification == Classification::single_player_or_coop);
		assert(summary.normal_levels == 1);
		assert(summary.robots == 0);
	}
	{
		Accumulator accumulator;
		accumulator.add(level(1, 1));
		accumulator.add(level(2, 8));
		const auto summary = accumulator.finish(ModeDeclarations());
		assert(summary.classification == Classification::ambiguous);
	}
	return 0;
}
