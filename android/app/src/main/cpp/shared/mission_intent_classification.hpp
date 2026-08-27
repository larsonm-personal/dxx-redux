#ifndef DXX_MISSION_INTENT_CLASSIFICATION_HPP
#define DXX_MISSION_INTENT_CLASSIFICATION_HPP

#include <algorithm>
#include <cstring>

namespace dxx_mission_intent
{
enum class Classification {
	single_player_or_coop,
	multiplayer_anarchy,
	single_player_or_coop_and_multiplayer_anarchy,
	ambiguous,
};

struct ModeDeclarations {
	bool anarchy_only = false;
	bool normal = false;
	bool coop = false;
	bool anarchy = false;
	bool robo_anarchy = false;
	bool capture_flag = false;
	bool hoard = false;

	void add(const char *value)
	{
		if (!value)
			return;
		if (!strcmp(value, "normal"))
			normal = true;
		else if (!strcmp(value, "coop"))
			coop = true;
		else if (!strcmp(value, "anarchy"))
			anarchy = true;
		else if (!strcmp(value, "robo_anarchy"))
			robo_anarchy = true;
		else if (!strcmp(value, "capture_flag"))
			capture_flag = true;
		else if (!strcmp(value, "hoard"))
			hoard = true;
	}

	bool campaign_declared() const { return normal || coop; }
	bool competitive_declared() const
	{
		return anarchy || robo_anarchy || capture_flag || hoard;
	}
};

struct LevelInputs {
	int level_num = 0;
	int player_starts = 0;
	int coop_starts = 0;
	int robots = 0;
	int hostages = 0;
	int matcens = 0;
	int guidebots = 0;
	int powerups = 0;
	int reactors = 0;
};

struct Summary {
	Classification classification = Classification::ambiguous;
	const char *rule = "missing_evidence";
	const char *confidence = "low";
	const char *reason = "No successfully analyzed normal levels supplied enough evidence";
	ModeDeclarations declarations;
	int normal_levels = 0;
	int campaign_actor_levels = 0;
	int arena_like_levels = 0;
	int solo_like_levels = 0;
	int player_start_min = 0;
	int player_start_max = 0;
	int coop_start_min = 0;
	int coop_start_max = 0;
	int robots = 0;
	int hostages = 0;
	int matcens = 0;
	int guidebots = 0;
	int powerups = 0;
	int reactors = 0;
};

inline const char *classification_name(Classification value)
{
	switch (value) {
		case Classification::single_player_or_coop: return "single_player_or_coop";
		case Classification::multiplayer_anarchy: return "multiplayer_anarchy";
		case Classification::single_player_or_coop_and_multiplayer_anarchy: return "single_player_or_coop_and_multiplayer_anarchy";
		case Classification::ambiguous: return "ambiguous";
	}
	return "ambiguous";
}

class Accumulator {
	Summary summary_;

public:
	void add(const LevelInputs &level)
	{
		if (level.level_num <= 0)
			return;
		const bool campaign_actors =
		    level.robots > 0 || level.hostages > 0 || level.matcens > 0 || level.guidebots > 0;
		if (!summary_.normal_levels) {
			summary_.player_start_min = level.player_starts;
			summary_.coop_start_min = level.coop_starts;
		} else {
			summary_.player_start_min = (std::min)(summary_.player_start_min, level.player_starts);
			summary_.coop_start_min = (std::min)(summary_.coop_start_min, level.coop_starts);
		}
		summary_.player_start_max = (std::max)(summary_.player_start_max, level.player_starts);
		summary_.coop_start_max = (std::max)(summary_.coop_start_max, level.coop_starts);
		++summary_.normal_levels;
		if (campaign_actors)
			++summary_.campaign_actor_levels;
		else if (level.player_starts >= 2)
			++summary_.arena_like_levels;
		else
			++summary_.solo_like_levels;
		summary_.robots += level.robots;
		summary_.hostages += level.hostages;
		summary_.matcens += level.matcens;
		summary_.guidebots += level.guidebots;
		summary_.powerups += level.powerups;
		summary_.reactors += level.reactors;
	}

	Summary finish(const ModeDeclarations &declarations) const
	{
		Summary result = summary_;
		result.declarations = declarations;
		if (declarations.anarchy_only) {
			result.classification = Classification::multiplayer_anarchy;
			result.rule = "descriptor_anarchy_only";
			result.confidence = "high";
			result.reason = "The mission descriptor marks this mission as anarchy-only";
		} else if (declarations.campaign_declared() && declarations.competitive_declared()) {
			result.classification = Classification::single_player_or_coop_and_multiplayer_anarchy;
			result.rule = "descriptor_both";
			result.confidence = "high";
			result.reason = "The mission descriptor explicitly enables campaign/cooperative and competitive modes";
		} else if (declarations.competitive_declared()) {
			result.classification = Classification::multiplayer_anarchy;
			result.rule = "descriptor_competitive";
			result.confidence = "high";
			result.reason = "The mission descriptor explicitly enables competitive multiplayer modes";
		} else if (declarations.campaign_declared()) {
			result.classification = Classification::single_player_or_coop;
			result.rule = "descriptor_campaign";
			result.confidence = "high";
			result.reason = "The mission descriptor explicitly enables normal or cooperative play";
		} else if (result.campaign_actor_levels > 0) {
			result.classification = Classification::single_player_or_coop;
			result.rule = "campaign_actors";
			result.confidence = "high";
			result.reason = "At least one normal level contains robots, hostages, matcens, or Guide-Bots";
		} else if (result.normal_levels > 0 && result.arena_like_levels == result.normal_levels) {
			result.classification = Classification::multiplayer_anarchy;
			result.rule = "all_levels_arena_like";
			result.confidence = "medium";
			result.reason = "Every normal level has multiple player starts and no campaign actors";
		} else if (result.normal_levels > 0 && result.solo_like_levels == result.normal_levels) {
			result.classification = Classification::single_player_or_coop;
			result.rule = "all_levels_solo_like";
			result.confidence = "medium";
			result.reason = "Every normal level without campaign actors has at most one player start";
		} else {
			result.classification = Classification::ambiguous;
			result.rule = "mixed_or_missing_evidence";
			result.confidence = "low";
			result.reason = "The normal levels contain mixed or incomplete structural evidence";
		}
		return result;
	}
};

template <typename Json>
Json serialize(const Summary &summary)
{
	Json declarations;
	declarations["anarchy_only"] = summary.declarations.anarchy_only;
	declarations["normal"] = summary.declarations.normal;
	declarations["coop"] = summary.declarations.coop;
	declarations["anarchy"] = summary.declarations.anarchy;
	declarations["robo_anarchy"] = summary.declarations.robo_anarchy;
	declarations["capture_flag"] = summary.declarations.capture_flag;
	declarations["hoard"] = summary.declarations.hoard;

	Json result;
	result["classification"] = classification_name(summary.classification);
	result["rule"] = summary.rule;
	result["confidence"] = summary.confidence;
	result["reason"] = summary.reason;
	result["declarations"] = declarations;
	result["normal_levels"] = summary.normal_levels;
	result["campaign_actor_levels"] = summary.campaign_actor_levels;
	result["arena_like_levels"] = summary.arena_like_levels;
	result["solo_like_levels"] = summary.solo_like_levels;
	result["player_start_min"] = summary.player_start_min;
	result["player_start_max"] = summary.player_start_max;
	result["coop_start_min"] = summary.coop_start_min;
	result["coop_start_max"] = summary.coop_start_max;
	result["robots"] = summary.robots;
	result["hostages"] = summary.hostages;
	result["matcens"] = summary.matcens;
	result["guidebots"] = summary.guidebots;
	result["powerups"] = summary.powerups;
	result["reactors"] = summary.reactors;
	return result;
}
}

#endif
