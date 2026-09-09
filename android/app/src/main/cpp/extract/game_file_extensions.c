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

/*
 * ISO/BIN data-track extraction should keep gameplay-relevant content while
 * skipping installer/runtime trees such as winsetup and directx.
 * These entries omit the leading '.' because iso9660_reader.c compares against
 * the raw extension text after the dot.
 */
const char *dxx_android_disc_extract_extensions[] = {
	"hog", "pig", "ham", "s11", "s22", "dem",
	"mvl", "msn", "mn2", "rdl", "rl2", "sdl", "sl2",
	"sow", "dxa", "pog", "hxm", "dtx",
	"txt", "tex", "txb", "ctb", "sng", "pcx", "hmp", "hmq", "mid", "256", "vham",
	"wav", "ogg", "mp3", "flac", "m3u",
	NULL
};

/* GOG CD-audio pair files live inside the broader game-file list above */
const char *dxx_android_gog_audio_extensions[] = {
	".gog", ".inst",
	NULL
};

const char *dxx_android_disc_companion_extensions[] = {
	".txt", ".tex", ".txb", ".ctb", ".sng", ".pcx", ".hmp", ".hmq", ".mid", ".256", ".vham",
	".wav", ".ogg", ".mp3", ".flac", ".m3u", NULL
};

/*
 * Mac HFS/STI archives need auxiliary config, text, and palette files and do
 * not use the generic ISO data-track allowlist. GameFileFormats.kt owns this
 * explicit media-specific role.
 */
const char *dxx_android_mac_disc_extract_extensions[] = {
	"hog", "ham", "pig", "s11", "s22", "mn2", "mvl", "sow",
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

int dxx_has_android_container_file_extension(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return 0;
	if (dxx_is_android_gog_audio_extension(path)) return 1;
	for (const char **ext = dxx_android_disc_extract_extensions; *ext; ext++) {
		if (dxx_ci_cmp(dot + 1, *ext) == 0) return 1;
	}
	return 0;
}
