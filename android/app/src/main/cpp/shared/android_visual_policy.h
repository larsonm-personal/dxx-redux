#ifndef ANDROID_VISUAL_POLICY_H
#define ANDROID_VISUAL_POLICY_H

#ifdef ANDROID

#include "game.h"

static inline int android_visual_replacements_allowed(void)
{
	return !(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_COOP);
}

#endif

#endif
