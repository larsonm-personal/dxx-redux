/* Exercise the production goal cache/transport with two simulated player views */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pstypes.h"
#include "fix.h"
#include "guidebot_goal_message.h"

#define NETWORK 1
#define GM_MULTI 2
#define GM_MULTI_COOP 4
#define MAX_PLAYERS 8
#define CONNECT_PLAYING 1
#define OF_EXPLODING 1
#define OF_SHOULD_BE_DEAD 2
#define OF_DESTROYED 4
#define MULTI_GUIDEBOT_GOAL 99
#define CC_COLOR 1
#define BM_XRGB(r,g,b) ((r)+(g)+(b))
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static int Game_mode, Player_num, N_players = 3, Buddy_objnum, Buddy_allowed_to_talk;
static int Escort_owner_player = 1;
static unsigned generation = 7;
static fix64 GameTime64;
static struct { int signature, flags, shields; } Objects[1] = {{42, 0, 100}};
static struct { int connected; } Players[MAX_PLAYERS] = {{1}, {1}, {1}};
static struct { const char *GuidebotName; } PlayerCfg = {"GUIDE"};
static ubyte packet[GUIDEBOT_GOAL_PACKET_LEN];
static int transient_messages, sent_messages;
static int escort_is_companion_object(int n) { return n == 0; }
static unsigned escort_get_owner_generation(void) { return generation; }
static int multi_who_is_master(void) { return 0; }
static int ok_for_buddy_to_talk(void) { return Buddy_allowed_to_talk; }
static void buddy_message(char *format, ...) { (void)format; ++transient_messages; }
static void multi_send_data(const ubyte *buf, int size, int priority) {
    CHECK(size == sizeof(packet) && priority == 2);
    memcpy(packet, buf, sizeof(packet)); ++sent_messages;
}
#include "guidebot_goal_message_impl.h"

int main(void)
{
    ubyte older[GUIDEBOT_GOAL_PACKET_LEN];
    escort_goal_message_reset();
    CHECK(!escort_goal_message_persistent());
    Buddy_allowed_to_talk = 1;
    buddy_goal_message("Staying away...");
    CHECK(transient_messages == 1 && !escort_goal_message());
    escort_set_goal_message_persistent(1);
    CHECK(strstr(escort_goal_message(), "Staying away"));
    buddy_goal_message("Coming back to get you.");
    CHECK(transient_messages == 1 && strstr(escort_goal_message(), "Coming back"));
    GameTime64 = 20 * F1_0;
    escort_goal_message_frame();
    CHECK(strstr(escort_goal_message(), "Coming back"));
    Objects[0].flags = OF_EXPLODING;
    CHECK(!escort_goal_message());
    escort_goal_message_frame();
    Objects[0].flags = 0; Objects[0].signature++;
    CHECK(!escort_goal_message());
    buddy_goal_message("Finding EXIT");
    CHECK(escort_goal_message());
    Buddy_allowed_to_talk = 0;
    escort_goal_message_frame(); CHECK(!escort_goal_message());
    Buddy_allowed_to_talk = 1;
    CHECK(!escort_goal_message());

    Game_mode = GM_MULTI | GM_MULTI_COOP; Player_num = 1;
    escort_goal_message_reset();
    escort_set_goal_message_persistent(0);
    buddy_goal_message("Staying away...");
    escort_goal_message_frame();
    CHECK(sent_messages == 1 && transient_messages == 2);
    memcpy(older, packet, sizeof(older));
    Objects[0].signature++;
    buddy_goal_message("Staying away...");
    escort_goal_message_frame();
    CHECK(GET_INTEL_INT(packet + 6) > GET_INTEL_INT(older + 6));
    buddy_goal_message("Coming back to get you.");
    escort_goal_message_frame();
    CHECK(sent_messages == 3);

    Player_num = 2; escort_goal_message_reset();
    escort_goal_message_receive(packet, 0); // Host relay, preference off
    CHECK(!escort_goal_message() && transient_messages == 4);
    escort_set_goal_message_persistent(1);
    CHECK(strstr(escort_goal_message(), "Coming back"));
    escort_goal_message_receive(older, 1);
    CHECK(strstr(escort_goal_message(), "Coming back"));
    Objects[0].flags = OF_SHOULD_BE_DEAD;
    escort_goal_message_frame();
    Objects[0].flags = 0; Objects[0].signature++;
    escort_goal_message_receive(packet, 0);
    CHECK(!escort_goal_message()); // Old snapshot cannot resurrect a pre-death goal
    generation++;
    escort_goal_message_frame(); escort_goal_message_receive(packet, 0);
    CHECK(!escort_goal_message());
    generation--; escort_goal_message_reset();
    escort_goal_message_receive(packet, 2); CHECK(!escort_goal_message());
    escort_goal_message_receive(packet, 1); CHECK(escort_goal_message());
    escort_goal_message_reset(); CHECK(!escort_goal_message());
    puts("Guidebot goal lifecycle and co-op transport checks passed");
    return 0;
}
