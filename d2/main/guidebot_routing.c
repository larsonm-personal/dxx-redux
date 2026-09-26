#include "inferno.h"
#include "guidebot_routing.h"
#include "playsave.h"
#include "input_demo_replay.h"
#include "ai.h"
#include "guidebot_route_internal.h"

static int Guidebot_routing_mode = GUIDEBOT_ROUTING_ENHANCED;
#ifdef __ANDROID__
static int Guidebot_routing_default = -1;
#endif

void guidebot_original_path_smoothing(vms_vector *velocity, const vms_vector *goal, fix frame_time)
{
	/* Redux converts the complete floating-point sum back to fixed point
	 * Truncating the increment first changes opposing velocity components */
	velocity->x += goal->x / 2 / ((float) (F1_0 / 30) / frame_time);
	velocity->y += goal->y / 2 / ((float) (F1_0 / 30) / frame_time);
	velocity->z += goal->z / 2 / ((float) (F1_0 / 30) / frame_time);
}

int guidebot_routing_valid(int mode)
{
	return mode == GUIDEBOT_ROUTING_ORIGINAL || mode == GUIDEBOT_ROUTING_ENHANCED;
}

int guidebot_routing_mode(void)
{
#ifdef NETWORK
	if (Game_mode & GM_MULTI_COOP)
		return Netgame.GuidebotRouting;
#endif
	return Guidebot_routing_mode;
}

int guidebot_routing_is_enhanced(void)
{
	return guidebot_routing_mode() == GUIDEBOT_ROUTING_ENHANCED;
}

const char *guidebot_routing_name(void)
{
	return guidebot_routing_is_enhanced() ? "Enhanced" : "Original";
}

int guidebot_routing_default(void)
{
#ifdef __ANDROID__
	const int requested = __atomic_load_n(&Guidebot_routing_default, __ATOMIC_RELAXED);
#else
	const int requested = PlayerCfg.GuidebotRouting;
#endif
	return guidebot_routing_valid(requested) ? requested : guidebot_routing_valid(PlayerCfg.GuidebotRouting) ? PlayerCfg.GuidebotRouting
	                                                                                                         : GUIDEBOT_ROUTING_ENHANCED;
}

void guidebot_routing_set_default(int mode)
{
	if (!guidebot_routing_valid(mode))
		return;
#ifdef __ANDROID__
	__atomic_store_n(&Guidebot_routing_default, mode, __ATOMIC_RELAXED);
#else
	PlayerCfg.GuidebotRouting = mode;
#endif
}

void guidebot_routing_set_mode(int mode)
{
	if (!guidebot_routing_valid(mode) || guidebot_routing_mode() == mode)
		return;
	Guidebot_routing_mode = mode;
#ifdef NETWORK
	if (Game_mode & GM_MULTI_COOP)
		Netgame.GuidebotRouting = (ubyte) mode;
#endif
	escort_reset_routing();
}

void guidebot_routing_start_session(void)
{
	input_demo_player_cfg recorded;
	guidebot_routing_set_mode(input_demo_replay_get_player_cfg(&recorded) ? recorded.guidebot_routing_mode : guidebot_routing_default());
}

void guidebot_routing_restore_mode(int mode)
{
	input_demo_player_cfg recorded;
	if (input_demo_replay_get_player_cfg(&recorded))
		mode = recorded.guidebot_routing_mode;
	if (!guidebot_routing_valid(mode))
		mode = guidebot_routing_default();
	Guidebot_routing_mode = mode;
#ifdef NETWORK
	/* A frozen secret world cannot choose a new session policy. Normal co-op
	 * save resume restores on the host; clients keep the synchronized policy */
	if ((Game_mode & GM_MULTI_COOP) && multi_i_am_master())
		Netgame.GuidebotRouting = (ubyte) mode;
#endif
#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	escort_route_reset_navigation();
#endif
}
