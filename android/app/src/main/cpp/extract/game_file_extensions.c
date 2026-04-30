#include "game_file_extensions.h"

#include <string.h>

#ifdef _WIN32
#include <string.h>
#define dxx_ci_cmp _stricmp
#else
#include <strings.h>
#define dxx_ci_cmp strcasecmp
#endif

/*
 * Generic game files discovered inside GOG installers and Mac .pkg payloads.
 * These retain the leading '.' because callers compare from the last dot in a
 * full filename or path.
 */
const char *dxx_android_game_file_extensions[] = {
	".hog", ".pig", ".ham", ".s11", ".s22", ".dem",
	".mvl", ".msn", ".mn2", ".gog", ".inst",
	NULL
};

/* GOG CD-audio pair files live inside the broader game-file list above */
const char *dxx_android_gog_audio_extensions[] = {
	".gog", ".inst",
	NULL
};

/*
 * Disc image extractors operate on extension suffixes without the leading '.'.
 * Keep these here so JNI and desktop helper tools do not drift apart.
 */
const char *dxx_android_disc_extract_extensions[] = {
	"hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
	"dxa", "cfg", "txt", "256", NULL
};

/* Mac HFS/STI archives also carry mission and demo files */
const char *dxx_android_mac_disc_extract_extensions[] = {
	"hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
	"dxa", "cfg", "txt", "256", "msn", "dem", NULL
};

int dxx_has_any_extension_ci(const char *path, const char *const *extensions)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return 0;
	for (const char *const *ext = extensions; *ext; ext++) {
		if (dxx_ci_cmp(dot, *ext) == 0) return 1;
	}
	return 0;
}

int dxx_has_android_game_file_extension(const char *path)
{
	return dxx_has_any_extension_ci(path, dxx_android_game_file_extensions);
}

int dxx_is_android_gog_audio_extension(const char *path)
{
	return dxx_has_any_extension_ci(path, dxx_android_gog_audio_extensions);
}