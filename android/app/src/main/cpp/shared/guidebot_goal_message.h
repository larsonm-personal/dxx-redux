#ifndef GUIDEBOT_GOAL_MESSAGE_H
#define GUIDEBOT_GOAL_MESSAGE_H

/* Shared wire text limit; includes the existing Guide-Bot name/color prefix */
#define GUIDEBOT_GOAL_MESSAGE_LEN 150
#define GUIDEBOT_GOAL_PACKET_LEN  (10 + GUIDEBOT_GOAL_MESSAGE_LEN)

void escort_set_goal_message_persistent(int enabled);
int escort_goal_message_persistent(void);
const char *escort_goal_message(void);
void escort_goal_message_reset(void);
void escort_goal_message_frame(void);
void escort_goal_message_receive(const unsigned char *buf, int sender);
void buddy_goal_message(char *format, ...);

#endif
