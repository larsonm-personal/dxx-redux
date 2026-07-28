#include <assert.h>
#include <stdio.h>

#include "game_file_extensions.h"

int main(void)
{
	assert(dxx_has_android_game_file_extension("DESCENT2.HOG"));
	assert(dxx_has_android_game_file_extension("missions/custom.Mn2"));
	assert(!dxx_has_android_game_file_extension("readme.txt"));
	assert(!dxx_has_android_game_file_extension("hog"));

	assert(dxx_is_android_gog_audio_extension("descent_ii.GOG"));
	assert(dxx_is_android_gog_audio_extension("audio/descENT_ii.InSt"));
	assert(!dxx_is_android_gog_audio_extension("descent2.hog"));
	assert(!dxx_is_android_gog_audio_extension("descent_ii.gog.bak"));

	printf("game file extension tests passed\n");
	return 0;
}
