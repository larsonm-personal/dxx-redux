#ifndef XL_REFERENCE_CONTACT_CONTROL_FLOW_H
#define XL_REFERENCE_CONTACT_CONTROL_FLOW_H
// Post-callback state is an input, as is FVI geometry in the native transcript
// These fragments retain the early return and the complete FixPosition guard
struct Contact : CObject {
	int bumps = 0;
	void DoBumpHack()
	{
		++bumps;
	}
	int completed_retry_result()
	{
#include "xl_reference_contact_death.h"
		// Alive controls supply completed movement, not the live wall callback
		// ProcessWallCollision normally returns 1 for a live ordinary wall hit
		return 0;
	}
	void reconstruct(CPhysSimData &simData)
	{
#include "xl_reference_contact_reconstruct.h"
	}
	void finish_wall_contact(CPhysSimData &simData)
	{
		int bRetry = completed_retry_result();
#include "xl_reference_contact_return.h"
		// A zero retry result exits the source loop and reaches FixPosition
		reconstruct(simData);
	}
};

namespace redux_contact
{
#ifdef DXX_XL_RUNTIME
#error The classic contact predicate requires a reference target without XL definitions
#endif
constexpr int OBJ_DEBRIS = 8;
struct Actor {
	int type = 0, flags = 0;
} Objects[1];
struct Hit {
	int hit_object = 0;
};
// These adapters supply the already-resolved fluid-helper results
bool wet = false, lava = false;
int xl_preview_water_segment(int)
{
	return wet;
}
int xl_lava_segment(int)
{
	return lava;
}
bool classic(Actor *obj, int fate)
{
#include "xl_redux_contact_predicate.h"
	return reconstruct_velocity;
}
#define DXX_XL_RUNTIME
bool xl(Actor *obj, int fate, bool speed_boost)
{
	const int orig_segnum = 0;
	Hit hit_info;
#include "xl_redux_contact_predicate.h"
	return reconstruct_velocity;
}
#undef DXX_XL_RUNTIME
} // namespace redux_contact

static int contact_control_flow()
{
	const auto same = [](const CFixVector &a, const CFixVector &b) {
		return a.v.coord.x == b.v.coord.x && a.v.coord.y == b.v.coord.y && a.v.coord.z == b.v.coord.z;
	};
	int source_cases = 0, predicate_cases = 0;
	gameData.physicsData.xTime = I2X(1) / 4;
	gameData.objData.pConsole = &objects[1];
	// Alive callers may reach FixPosition; dead wall callbacks return before it
	// Powerup cases examine the surviving-type gap without widening production
	for (int type : { OBJ_PLAYER, OBJ_ROBOT, 7 })
		for (int scale : { 100, 75, 66 })
			for (int exclusions = 0; exclusions < 8; ++exclusions)
				for (int fate : { HIT_NONE, HIT_WALL, HIT_OBJECT, HIT_BAD_P0 })
					for (int dead : { 0, OF_SHOULD_BE_DEAD }) {
						if (dead && fate != HIT_WALL) continue;
						Contact actor;
						actor.info.nType = type;
						actor.info.nFlags = dead;
						actor.info.position.vPos.Set(I2X(1) / 16, 0, 0);
						actor.Velocity().Set(I2X(4), 0, 0);
						const auto incoming = actor.Velocity();
						CPhysSimData state;
						state.xTimeScale = scale;
						state.hitResult.nType = fate;
						state.bSpeedBoost = exclusions & 1;
						state.bStopped = exclusions & 2;
						state.bBounced = exclusions & 4;
						if (fate == HIT_WALL)
							actor.finish_wall_contact(state);
						else
							actor.reconstruct(state);
						const bool reconstruct = !dead && !exclusions && (fate == HIT_WALL || fate == HIT_BAD_P0);
						CFixVector expected = incoming;
						if (reconstruct) expected.Set(fix((262144LL * scale / 100) / 16), 0, 0);
						if (!same(actor.Velocity(), expected) || actor.bumps)
							throw std::runtime_error("Pinned contact completion or exclusion differs");
						++source_cases;
					}

	// Compile the actual Redux predicate in both configurations, not a copy
	// Dry nonclassic survivors intentionally remain outside this correction
	for (int type : { OBJ_PLAYER, OBJ_ROBOT, redux_contact::OBJ_DEBRIS, 5, 7 })
		for (int fate : { HIT_NONE, HIT_WALL, HIT_OBJECT, HIT_BAD_P0 })
			for (int dead : { 0, OF_SHOULD_BE_DEAD })
				for (int medium : { 0, 1, 2 })
					for (int boost : { 0, 1 })
						for (int camera : { 0, 1 }) {
							redux_contact::Actor actor{ type, dead };
							redux_contact::Objects[0].type = camera ? 16 : OBJ_ROBOT;
							redux_contact::wet = medium == 1;
							redux_contact::lava = medium == 2;
							const bool classic = (type == OBJ_PLAYER || type == OBJ_ROBOT || type == redux_contact::OBJ_DEBRIS) && fate != HIT_NONE;
							bool xl = classic && !boost && !(camera && fate == HIT_OBJECT);
							if (medium && !boost) xl = fate == HIT_BAD_P0 || (!dead && fate == HIT_WALL);
							if (redux_contact::classic(&actor, fate) != classic || redux_contact::xl(&actor, fate, boost) != xl)
								throw std::runtime_error("Redux contact predicate changed outside the scoped correction");
							predicate_cases += 2;
						}
	std::cout << source_cases << " pinned contact completion cases and " << predicate_cases << " Redux predicate cases passed\n";
	return 0;
}
#endif
