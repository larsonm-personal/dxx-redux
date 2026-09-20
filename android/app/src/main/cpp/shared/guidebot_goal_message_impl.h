/* Included by escort.c so lifecycle checks use the engine's companion identity */
#include "guidebot_goal_message.h"
#include "guidebot_info_overlay.h"
#include "byteswap.h"

static int Escort_persist_goal_message;
static char Escort_goal_message_text[GUIDEBOT_GOAL_MESSAGE_LEN];
static int Escort_goal_message_signature = -1;
static int Escort_goal_message_owner = -1;
static unsigned Escort_goal_message_generation;
static unsigned Escort_goal_message_sequence;
static fix64 Escort_goal_message_next_send;

void escort_set_goal_message_persistent(int enabled)
{
#ifdef __ANDROID__
	__atomic_store_n(&Escort_persist_goal_message, !!enabled, __ATOMIC_RELAXED);
#else
	Escort_persist_goal_message = !!enabled;
#endif
}

int escort_goal_message_persistent(void)
{
#ifdef __ANDROID__
	return __atomic_load_n(&Escort_persist_goal_message, __ATOMIC_RELAXED);
#else
	return Escort_persist_goal_message;
#endif
}

void escort_goal_message_reset(void)
{
#ifdef __ANDROID__
	guidebot_info_reset();
#endif
	Escort_goal_message_text[0] = 0;
	Escort_goal_message_signature = -1;
	Escort_goal_message_owner = -1;
	Escort_goal_message_generation = 0;
	Escort_goal_message_sequence = 0;
	Escort_goal_message_next_send = 0;
}

static int escort_goal_message_live(void)
{
	return escort_is_companion_object(Buddy_objnum) && Buddy_allowed_to_talk &&
	       !(Objects[Buddy_objnum].flags & (OF_EXPLODING | OF_SHOULD_BE_DEAD | OF_DESTROYED)) &&
	       Objects[Buddy_objnum].shields >= 0;
}

static void escort_goal_message_sync_owner(void)
{
#ifdef NETWORK
	int owner = (Game_mode & GM_MULTI_COOP) ? Escort_owner_player : Player_num;
	unsigned generation = (Game_mode & GM_MULTI_COOP) ? escort_get_owner_generation() : 0;
#else
	int owner = Player_num;
	unsigned generation = 0;
#endif
	if (Escort_goal_message_owner != owner || Escort_goal_message_generation != generation) {
		escort_goal_message_reset();
		Escort_goal_message_owner = owner;
		Escort_goal_message_generation = generation;
	}
}

static void escort_goal_message_store(const char *message)
{
	escort_goal_message_sync_owner();
	if (strcmp(Escort_goal_message_text, message) ||
	    (message[0] && escort_goal_message_live() &&
	     Escort_goal_message_signature != Objects[Buddy_objnum].signature)) {
		snprintf(Escort_goal_message_text, sizeof(Escort_goal_message_text), "%s", message);
		++Escort_goal_message_sequence;
		Escort_goal_message_next_send = 0;
	}
	Escort_goal_message_signature = escort_goal_message_live() ? Objects[Buddy_objnum].signature : -1;
}

const char *escort_goal_message(void)
{
	if (!escort_goal_message_persistent() || !escort_goal_message_live() ||
	    Escort_goal_message_signature != Objects[Buddy_objnum].signature)
		return NULL;
	return Escort_goal_message_text[0] ? Escort_goal_message_text : NULL;
}

void escort_goal_message_frame(void)
{
	escort_goal_message_sync_owner();
	if (!escort_goal_message_live() || Escort_goal_message_signature != Objects[Buddy_objnum].signature) {
#ifdef NETWORK
		if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num) {
			/* Keep the last sequence as a barrier against pre-death snapshots */
			Escort_goal_message_text[0] = 0;
			Escort_goal_message_signature = -1;
		} else
#endif
			escort_goal_message_store("");
	}
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == Player_num &&
	    (Escort_goal_message_next_send <= GameTime64 || Escort_goal_message_next_send > GameTime64 + 2 * F1_0)) {
		ubyte buf[GUIDEBOT_GOAL_PACKET_LEN] = { 0 };
		buf[0] = MULTI_GUIDEBOT_GOAL;
		buf[1] = (ubyte) Player_num;
		PUT_INTEL_INT(buf + 2, Escort_goal_message_generation);
		PUT_INTEL_INT(buf + 6, Escort_goal_message_sequence);
		memcpy(buf + 10, Escort_goal_message_text, GUIDEBOT_GOAL_MESSAGE_LEN);
		multi_send_data(buf, sizeof(buf), 2);
		Escort_goal_message_next_send = GameTime64 + 2 * F1_0;
	}
#endif
}

void escort_goal_message_receive(const unsigned char *buf, int sender)
{
#ifdef NETWORK
	unsigned generation = GET_INTEL_INT(buf + 2);
	unsigned sequence = GET_INTEL_INT(buf + 6);
	int owner = buf[1];
	if (!(Game_mode & GM_MULTI_COOP) || owner >= N_players || owner >= MAX_PLAYERS ||
	    owner == Player_num || owner != Escort_owner_player ||
	    (sender != owner && sender != multi_who_is_master()) ||
	    generation != escort_get_owner_generation() || Players[owner].connected != CONNECT_PLAYING)
		return;
	escort_goal_message_sync_owner();
	/* Repeated snapshots seed late joiners but never replace a newer goal */
	if (sequence < Escort_goal_message_sequence ||
	    (sequence == Escort_goal_message_sequence && sequence && !Escort_goal_message_text[0]) ||
	    !escort_goal_message_live())
		return;
	memcpy(Escort_goal_message_text, buf + 10, GUIDEBOT_GOAL_MESSAGE_LEN);
	Escort_goal_message_text[GUIDEBOT_GOAL_MESSAGE_LEN - 1] = 0;
	Escort_goal_message_sequence = sequence;
	Escort_goal_message_signature = Objects[Buddy_objnum].signature;
#else
	(void) buf;
	(void) sender;
#endif
}

void buddy_goal_message(char *format, ...)
{
	char text[128];
	char message[GUIDEBOT_GOAL_MESSAGE_LEN];
	va_list args;
#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && (!(Game_mode & GM_MULTI_COOP) || Escort_owner_player != Player_num))
		return;
#endif
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
#ifdef __ANDROID__
	if (strstr(text, "Can't") || strstr(text, "can't") || strstr(text, "Cannot") ||
	    (!strncmp(text, "No ", 3) && strstr(text, "in mine")))
		guidebot_info_event(text, 1);
#endif
	if (ok_for_buddy_to_talk() && escort_goal_message_live()) {
		snprintf(message, sizeof(message), "%c%c%s:%c%c %s", CC_COLOR, BM_XRGB(28, 0, 0),
		         PlayerCfg.GuidebotName, CC_COLOR, BM_XRGB(0, 31, 0), text);
		escort_goal_message_store(message);
	}
	if (!escort_goal_message_persistent())
		buddy_message("%s", text);
}
