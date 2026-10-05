#ifndef ANDROID_BRIEFING_TEXT_H
#define ANDROID_BRIEFING_TEXT_H

/* Android diagnostics for the text actually retained on a briefing page */
typedef struct android_briefing_text_state {
	int active;
	int page_ready;
	int overlaps;
	char background[32];
	char text[2049];
} android_briefing_text_state;

void android_briefing_text_snapshot(android_briefing_text_state *state);

#endif
