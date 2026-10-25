// Included by native digiobj.c to inspect its owned sound registry without copying it
#if defined(__ANDROID__) && defined(USE_SDLMIXER)
#include <SDL_mixer.h>
#endif
int xl_linked_sound_count(int object_index, int sound, int playing_only)
{
	int i, count = 0;
	const int sample = digi_xlat_sound(sound);
	if (object_index < 0 || object_index > Highest_object_index || sample < 0) return 0;
	for (i = 0; i < MAX_SOUND_OBJECTS; ++i) {
		const sound_object *entry = &SoundObjects[i];
		if ((entry->flags & (SOF_USED | SOF_LINK_TO_OBJ)) != (SOF_USED | SOF_LINK_TO_OBJ) ||
		    entry->link_type.obj.objnum != object_index || entry->link_type.obj.objsignature != Objects[object_index].signature ||
		    entry->soundnum != sample) continue;
		if (playing_only) {
			if (entry->channel < 0) continue;
#if defined(__ANDROID__) && defined(USE_SDLMIXER)
			// The legacy mixer digi_is_channel_playing wrapper is a stub
			if (!Mix_Playing(entry->channel)) continue;
#else
			if (!digi_is_channel_playing(entry->channel)) continue;
#endif
		}
		++count;
	}
	return count;
}
