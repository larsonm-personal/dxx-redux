#ifndef DXX_ANDROID_EXTRACT_GAME_FILE_EXTENSIONS_H
#define DXX_ANDROID_EXTRACT_GAME_FILE_EXTENSIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native mirrors of the extension roles owned by GameFileFormats.kt.
 * AndroidGameFileExtensionsTest mechanically compares every table with that
 * authoritative policy. The Mac table is a documented media-specific role,
 * not a second copy of the generic disc policy.
 */

extern const char *dxx_android_game_file_extensions[];
extern const char *dxx_android_disc_extract_extensions[];
extern const char *dxx_android_gog_audio_extensions[];
extern const char *dxx_android_mac_disc_extract_extensions[];

int dxx_has_any_extension_ci(const char *path, const char *const *extensions);
int dxx_has_android_game_file_extension(const char *path);
int dxx_is_android_gog_audio_extension(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DXX_ANDROID_EXTRACT_GAME_FILE_EXTENSIONS_H */
