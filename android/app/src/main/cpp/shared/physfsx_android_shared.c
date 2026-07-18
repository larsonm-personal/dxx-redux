/* Shared Android PhysFS search-path initialization for D1 and D2. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "physfsx.h"

#include "physfsx_android_shared.h"

extern const PHYSFS_Archiver SAF_Archiver;

void physfsx_android_init_search_paths(const char *game_dir)
{
	const char *pref = PHYSFS_getPrefDir("com.dxxredux", game_dir);
	const char *preview_setdir = getenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR");
	if (pref) {
		/* Keep each game's pilots, saves, and configs isolated. */
		char gamedir[512];
		snprintf(gamedir, sizeof(gamedir), "%s%s/", pref, game_dir);
		PHYSFS_setWriteDir(pref);
		PHYSFS_mkdir(game_dir);
		PHYSFS_setWriteDir(gamedir);
		PHYSFS_addToSearchPath(gamedir, 1);
		PHYSFS_addToSearchPath(pref, 1);
	}

	/* Register manifests that leave Storage Access Framework files in place. */
	PHYSFS_registerArchiver(&SAF_Archiver);

	/* Prepend the active file set, then append its SAF manifest if present. */
	if (pref) {
		char asp[512];
		char setdir[512] = "";
		char safpath[512];
		char gd[512];
		snprintf(gd, sizeof(gd), "%s%s/", pref, game_dir);
		snprintf(asp, sizeof(asp), "%s.active_set_path", gd);
		if (preview_setdir && preview_setdir[0])
			snprintf(setdir, sizeof(setdir), "%s", preview_setdir);
		else {
			FILE *f = fopen(asp, "r");
			if (f) {
				if (fgets(setdir, sizeof(setdir), f)) {
					char *nl = strchr(setdir, '\n');
					if (nl) *nl = '\0';
				}
				fclose(f);
			}
		}
		if (strlen(setdir) > 0)
			PHYSFS_addToSearchPath(setdir, 0); /* prepend */

		if (strlen(setdir) > 0)
			snprintf(safpath, sizeof(safpath), "%s/.saf_manifest.json", setdir);
		else
			snprintf(safpath, sizeof(safpath), "%s.saf_manifest.json", pref);

		{
			FILE *sf = fopen(safpath, "r");
			if (sf) {
				fclose(sf);
				PHYSFS_mount(safpath, NULL, 1); /* append after filesDir */
			}
		}
	}

	PHYSFS_addToSearchPath(PHYSFS_getBaseDir(), 1);
	PHYSFSX_addRelToSearchPath("data", 1);

	/* Reverse-prepend enabled mods so their final order matches the UI. */
	if (pref && (!preview_setdir || !preview_setdir[0])) {
		char modpath[512];
		char mod_lines[64][512];
		char mod_mounts[64][64];
		int mod_count = 0;
		snprintf(modpath, sizeof(modpath), "%s%s/.active_mod_paths", pref, game_dir);
		FILE *mf = fopen(modpath, "r");
		if (mf) {
			char line[512];
			int i;
			while (fgets(line, sizeof(line), mf)) {
				char *nl = strpbrk(line, "\r\n");
				if (nl) *nl = '\0';
				if (strlen(line) > 0 && mod_count < (int) (sizeof(mod_lines) / sizeof(mod_lines[0]))) {
					char *mount = strchr(line, '\t');
					if (mount) {
						*mount++ = '\0';
						snprintf(mod_mounts[mod_count], sizeof(mod_mounts[mod_count]), "%s", mount);
					} else {
						mod_mounts[mod_count][0] = '\0';
					}
					snprintf(mod_lines[mod_count], sizeof(mod_lines[mod_count]), "%s", line);
					mod_count++;
				}
			}
			fclose(mf);
			for (i = mod_count - 1; i >= 0; i--) {
				const char *mount_point = mod_mounts[i][0] ? mod_mounts[i] : NULL;
				if (PHYSFS_mount(mod_lines[i], mount_point, 0))
					con_printf(CON_NORMAL, "PHYSFS: Mounted mod %s%s%s\n", mod_lines[i], mount_point ? " at " : "", mount_point ? mount_point : "");
				else
					con_printf(CON_NORMAL, "PHYSFS: Failed to mount mod %s: %s\n", mod_lines[i], PHYSFS_getLastError());
			}
		} else {
			con_printf(CON_NORMAL, "PHYSFS: No .active_mod_paths at %s\n", modpath);
		}
	}
}
