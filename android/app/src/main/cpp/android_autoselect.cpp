/*
 * android_autoselect.cpp -- Read/write weapon autoselect ordering from
 * pilot files (.plr/.plx) via JNI, for the launcher's autoselect editor.
 *
 * D1: weapon ordering stored as text INI in .plx file (sibling of .plr)
 * D2: weapon ordering stored as binary in .plr file (interleaved bytes)
 *
 * All pilot file format knowledge stays in C (alongside playsave.c).
 * Kotlin calls these JNI functions and gets/sets flat int arrays.
 */

#ifdef ANDROID

#include <jni.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>
#include <android/log.h>

extern "C" {
#include "playsave.h"
#include "weapon.h"
#include "kconfig.h"
}

#ifdef DXX_BUILD_DESCENT_II
#include "multi.h"
#include "makesig.h"
/* These constants are private #defines in playsave.c; duplicate here.
 * Shared constant: keep in sync with playsave.c */
#define AUTOSEL_SAVE_FILE_ID    MAKE_SIG('D', 'P', 'L', 'R')
#define AUTOSEL_MIN_PLR_VERSION 17
#endif

#define LOG_TAG   "DXX-Autoselect"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── Weapon name tables (hardcoded English, matching in-game reorder menus) ── */

#ifdef DXX_BUILD_DESCENT_II

static const char *d2_primary_names[] = {
	"Laser Cannon",       /* 0 */
	"Vulcan Cannon",      /* 1 */
	"Spreadfire Cannon",  /* 2 */
	"Plasma Cannon",      /* 3 */
	"Fusion Cannon",      /* 4 */
	"Super Laser Cannon", /* 5 */
	"Gauss Cannon",       /* 6 */
	"Helix Cannon",       /* 7 */
	"Phoenix Cannon",     /* 8 */
	"Omega Cannon",       /* 9 */
};

static const char *d2_secondary_names[] = {
	"Concussion Missile",  /* 0 */
	"Homing Missile",      /* 1 */
	"Proximity Bomb",      /* 2 */
	"Smart Missile",       /* 3 */
	"Mega Missile",        /* 4 */
	"Flash Missile",       /* 5 */
	"Guided Missile",      /* 6 */
	"Smart Mine",          /* 7 */
	"Mercury Missile",     /* 8 */
	"Earthshaker Missile", /* 9 */
};

#else /* D1 */

static const char *d1_primary_names[] = {
	"Laser Cannon",      /* 0 */
	"Vulcan Cannon",     /* 1 */
	"Spreadfire Cannon", /* 2 */
	"Plasma Cannon",     /* 3 */
	"Fusion Cannon",     /* 4 */
};

static const char *d1_secondary_names[] = {
	"Concussion Missile", /* 0 */
	"Homing Missile",     /* 1 */
	"Proximity Bomb",     /* 2 */
	"Smart Missile",      /* 3 */
	"Mega Missile",       /* 4 */
};

/* D1 special: index 16 = Quad Lasers (hardcoded string, matching in-game menu) */
static const char D1_QUAD_LASERS_NAME[] = "Quad Lasers";

#endif /* D1 vs D2 names */

/* ── Default orderings (matching DefaultPrimaryOrder/DefaultSecondaryOrder in weapon.c) ── */

/* D1: primary has 7 entries (5 weapons + separator + quad), secondary has 6 */
#define D1_PRIMARY_ORDER_LEN   7 /* MAX_PRIMARY_WEAPONS(5) + 2 */
#define D1_SECONDARY_ORDER_LEN 6 /* MAX_SECONDARY_WEAPONS(5) + 1 */

/* D2: primary and secondary each have 11 entries (10 weapons + separator) */
#define D2_PRIMARY_ORDER_LEN   11 /* MAX_PRIMARY_WEAPONS(10) + 1 */
#define D2_SECONDARY_ORDER_LEN 11 /* MAX_SECONDARY_WEAPONS(10) + 1 */

#ifdef DXX_BUILD_DESCENT_II
static const unsigned char d2_default_primary[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255 };
static const unsigned char d2_default_secondary[] = { 9, 8, 4, 3, 1, 5, 0, 255, 7, 6, 2 };
#else
static const unsigned char d1_default_primary[] = { 4, 3, 2, 1, 0, 255, 16 };
static const unsigned char d1_default_secondary[] = { 4, 3, 1, 0, 255, 2 };
#endif

/* ══════════════════════════════════════════════════════════════════════
 * D2: Binary .plr read/write
 * ══════════════════════════════════════════════════════════════════════ */

#ifdef DXX_BUILD_DESCENT_II

/* Compute the file offset of weapon ordering bytes in a D2 .plr file.
 * Returns offset on success, -1 on failure.  Mirrors plr_patch_keysettings(). */
static long d2_weapon_order_offset(FILE *f)
{
	unsigned char buf[4];
	unsigned int id;
	int ver, n_highest, fixed_header;

	if (fread(buf, 1, 4, f) != 4) return -1;
	id = (unsigned) buf[0] | ((unsigned) buf[1] << 8) |
	     ((unsigned) buf[2] << 16) | ((unsigned) buf[3] << 24);
	if (id != (unsigned) AUTOSEL_SAVE_FILE_ID) return -1;

	if (fread(buf, 1, 2, f) != 2) return -1;
	ver = buf[0] | (buf[1] << 8);
	if (ver < AUTOSEL_MIN_PLR_VERSION) return -1;

	fixed_header = (ver >= 19) ? 19 : 18;

	fseek(f, fixed_header, SEEK_SET);
	if (fread(buf, 1, 2, f) != 2) return -1;
	n_highest = buf[0] | (buf[1] << 8);

	long ks_base = fixed_header + 2 + (long) n_highest * sizeof(hli) + 4 * MAX_MESSAGE_LEN;

	/* Weapon order is at: ks_base + 8*MAX_CONTROLS + 3
	 * (past keyboard, joystick, 3*obsolete, mouse, 2*obsolete,
	 *  control_type_dos, control_type_win, avg_joy_sensitivity) */
	long offset = ks_base + 8 * MAX_CONTROLS + 3;

	/* Verify file is large enough for 22 interleaved weapon bytes */
	fseek(f, 0, SEEK_END);
	if (ftell(f) < offset + 22) return -1;

	return offset;
}

/* Read D2 weapon ordering from a .plr file.
 * Fills primary[11] and secondary[11].  Returns 1 on success. */
static int d2_read_weapon_order(const char *path,
                                unsigned char *primary, unsigned char *secondary)
{
	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	long offset = d2_weapon_order_offset(f);
	if (offset < 0) {
		fclose(f);
		return 0;
	}

	fseek(f, offset, SEEK_SET);
	for (int i = 0; i < D2_PRIMARY_ORDER_LEN; i++) {
		int p = fgetc(f);
		int s = fgetc(f);
		if (p == EOF || s == EOF) {
			fclose(f);
			return 0;
		}
		primary[i] = (unsigned char) p;
		secondary[i] = (unsigned char) s;
	}

	fclose(f);
	return 1;
}

/* Write D2 weapon ordering to a .plr file.
 * Writes interleaved primary[i], secondary[i].  Returns 1 on success. */
static int d2_write_weapon_order(const char *path,
                                 const unsigned char *primary,
                                 const unsigned char *secondary)
{
	FILE *f = fopen(path, "r+b");
	if (!f) return 0;

	long offset = d2_weapon_order_offset(f);
	if (offset < 0) {
		fclose(f);
		return 0;
	}

	fseek(f, offset, SEEK_SET);
	for (int i = 0; i < D2_PRIMARY_ORDER_LEN; i++) {
		fputc(primary[i], f);
		fputc(secondary[i], f);
	}

	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return 1;
}

#else /* D1 */

/* ══════════════════════════════════════════════════════════════════════
 * D1: Text .plx read/write
 * ══════════════════════════════════════════════════════════════════════ */

/* Read D1 weapon ordering from a .plx file.
 * Fills primary[7] and secondary[6].  Returns 1 on success. */
static int d1_read_weapon_order(const char *plx_path,
                                unsigned char *primary, unsigned char *secondary)
{
	FILE *f = fopen(plx_path, "r");
	if (!f) return 0;

	/* Set defaults in case file doesn't contain weapon reorder section */
	memcpy(primary, d1_default_primary, D1_PRIMARY_ORDER_LEN);
	memcpy(secondary, d1_default_secondary, D1_SECONDARY_ORDER_LEN);

	char line[256];
	int in_reorder = 0;
	while (fgets(line, sizeof(line), f)) {
		/* Case-insensitive match for section header */
		if (strstr(line, "[weapon reorder]") || strstr(line, "[WEAPON REORDER]")) {
			in_reorder = 1;
			continue;
		}
		if (in_reorder && (strstr(line, "[end]") || strstr(line, "[END]"))) {
			break;
		}
		if (!in_reorder) continue;

		/* Parse primary= or secondary= line */
		if (strncmp(line, "primary=", 8) == 0) {
			unsigned int w[7] = { 0 };
			int n = sscanf(line + 8,
			               "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x",
			               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6]);
			if (n >= 6) {
				for (int i = 0; i < D1_PRIMARY_ORDER_LEN; i++)
					primary[i] = (unsigned char) w[i];
			}
		} else if (strncmp(line, "secondary=", 10) == 0) {
			unsigned int w[6] = { 0 };
			int n = sscanf(line + 10,
			               "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x",
			               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5]);
			if (n >= 5) {
				for (int i = 0; i < D1_SECONDARY_ORDER_LEN; i++)
					secondary[i] = (unsigned char) w[i];
			}
		}
	}

	fclose(f);
	return 1;
}

/* Write D1 weapon ordering to a .plx file.
 * Reads the existing .plx, replaces the [weapon reorder] section, writes back.
 * Returns 1 on success. */
static int d1_write_weapon_order(const char *plx_path,
                                 const unsigned char *primary,
                                 const unsigned char *secondary)
{
	/* Read existing file into memory (typical plx files are tiny) */
	FILE *f = fopen(plx_path, "r");

	/* Buffer for reconstructed file */
	char buf[4096];
	int buf_len = 0;
	int in_reorder = 0;
	int wrote_reorder = 0;

/* Helper: append string to buffer */
#define BUF_APPEND(s)                              \
	do {                                           \
		int _slen = (int) strlen(s);               \
		if (buf_len + _slen < (int) sizeof(buf)) { \
			memcpy(buf + buf_len, s, _slen);       \
			buf_len += _slen;                      \
		}                                          \
	} while (0)

	/* Write the weapon reorder section */
	auto write_reorder_section = [&]() {
		char tmp[256];
		BUF_APPEND("[weapon reorder]\n");
		snprintf(tmp, sizeof(tmp),
		         "primary=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		         primary[0], primary[1], primary[2], primary[3],
		         primary[4], primary[5], primary[6]);
		BUF_APPEND(tmp);
		snprintf(tmp, sizeof(tmp),
		         "secondary=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		         secondary[0], secondary[1], secondary[2],
		         secondary[3], secondary[4], secondary[5]);
		BUF_APPEND(tmp);
		BUF_APPEND("[end]\n");
		wrote_reorder = 1;
	};

	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (strstr(line, "[weapon reorder]") || strstr(line, "[WEAPON REORDER]")) {
				in_reorder = 1;
				write_reorder_section();
				continue;
			}
			if (in_reorder) {
				if (strstr(line, "[end]") || strstr(line, "[END]")) {
					in_reorder = 0;
				}
				continue; /* skip old reorder content */
			}
			BUF_APPEND(line);
		}
		fclose(f);
	}

	if (!wrote_reorder) {
		/* File didn't have a reorder section (or didn't exist); prepend header */
		if (buf_len == 0) {
			BUF_APPEND("[D1X Options]\n");
		}
		write_reorder_section();
	}

#undef BUF_APPEND

	/* Write buffer to file */
	f = fopen(plx_path, "w");
	if (!f) return 0;
	fwrite(buf, 1, buf_len, f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return 1;
}

#endif /* D1 vs D2 */

/* ══════════════════════════════════════════════════════════════════════
 * Shared: scan for pilot files in the game's Players directory
 * ══════════════════════════════════════════════════════════════════════ */

/* Find the first file with the given extension in the game directories.
 * D2 uses ".plr" (binary), D1 uses ".plx" (text weapon ordering).
 * Returns 1 and fills path_out on success. */
static int find_first_pilot(const char *files_dir, const char *subdir,
                            const char *ext, char *path_out, size_t path_size)
{
	char players_dir[512], base_dir[512];
	size_t ext_len = strlen(ext);
	snprintf(base_dir, sizeof(base_dir), "%s/%s", files_dir, subdir);
	snprintf(players_dir, sizeof(players_dir), "%s/%s/Players", files_dir, subdir);

	const char *dirs[] = { base_dir, players_dir };
	for (int d = 0; d < 2; d++) {
		DIR *dp = opendir(dirs[d]);
		if (!dp) continue;
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL) {
			size_t nlen = strlen(ent->d_name);
			if (nlen < ext_len + 1) continue;
			if (strcasecmp(ent->d_name + nlen - ext_len, ext) != 0) continue;
			snprintf(path_out, path_size, "%s/%s", dirs[d], ent->d_name);
			closedir(dp);
			return 1;
		}
		closedir(dp);
	}
	return 0;
}

/* Iterate over all files with the given extension calling a visitor function.
 * Returns count of successful visits. */
typedef int (*plr_visitor_fn)(const char *path, void *ctx);

static int for_each_pilot(const char *files_dir, const char *subdir,
                          const char *ext, plr_visitor_fn visitor, void *ctx)
{
	char players_dir[512], base_dir[512];
	size_t ext_len = strlen(ext);
	snprintf(base_dir, sizeof(base_dir), "%s/%s", files_dir, subdir);
	snprintf(players_dir, sizeof(players_dir), "%s/%s/Players", files_dir, subdir);

	int total = 0;
	const char *dirs[] = { base_dir, players_dir };
	for (int d = 0; d < 2; d++) {
		DIR *dp = opendir(dirs[d]);
		if (!dp) continue;
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL) {
			size_t nlen = strlen(ent->d_name);
			if (nlen < ext_len + 1) continue;
			if (strcasecmp(ent->d_name + nlen - ext_len, ext) != 0) continue;
			char path[512];
			snprintf(path, sizeof(path), "%s/%s", dirs[d], ent->d_name);
			total += visitor(path, ctx);
		}
		closedir(dp);
	}
	return total;
}

/* ══════════════════════════════════════════════════════════════════════
 * JNI entry points
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * nativeReadAutoselect(filesDir) -> int[]
 * Returns flat array: [primary..., secondary...]
 * D1: 7 primary + 6 secondary = 13 ints
 * D2: 11 primary + 11 secondary = 22 ints
 * Returns empty array if no pilot file found.
 */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_NativeAutoselectPatcher_nativeReadAutoselect(
    JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

#ifdef DXX_BUILD_DESCENT_II
	const char *subdir = "d2x-redux";
	const char *ext = ".plr";
	const int prim_len = D2_PRIMARY_ORDER_LEN;
	const int sec_len = D2_SECONDARY_ORDER_LEN;
#else
	const char *subdir = "d1x-redux";
	const char *ext = ".plx";
	const int prim_len = D1_PRIMARY_ORDER_LEN;
	const int sec_len = D1_SECONDARY_ORDER_LEN;
#endif

	char pilot_path[512];
	if (!find_first_pilot(files_dir, subdir, ext, pilot_path, sizeof(pilot_path))) {
		LOGI("nativeReadAutoselect: no pilot file found in %s/%s", files_dir, subdir);
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return env->NewIntArray(0);
	}

	unsigned char primary[D2_PRIMARY_ORDER_LEN];
	unsigned char secondary[D2_SECONDARY_ORDER_LEN];
	int ok;

#ifdef DXX_BUILD_DESCENT_II
	ok = d2_read_weapon_order(pilot_path, primary, secondary);
#else
	ok = d1_read_weapon_order(pilot_path, primary, secondary);
#endif

	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	if (!ok) {
		LOGE("nativeReadAutoselect: failed to read weapon order from %s", pilot_path);
		return env->NewIntArray(0);
	}

	LOGI("nativeReadAutoselect: read from %s", pilot_path);

	int total = prim_len + sec_len;
	jintArray result = env->NewIntArray(total);
	jint flat[D2_PRIMARY_ORDER_LEN + D2_SECONDARY_ORDER_LEN];
	for (int i = 0; i < prim_len; i++) flat[i] = primary[i];
	for (int i = 0; i < sec_len; i++) flat[prim_len + i] = secondary[i];
	env->SetIntArrayRegion(result, 0, total, flat);
	return result;
}

/*
 * nativeWriteAutoselect(filesDir, primaryOrder, secondaryOrder) -> int
 * Writes ordering to ALL .plr files for this game.
 * Returns number of files patched.
 */
struct write_ctx {
	const unsigned char *primary;
	const unsigned char *secondary;
};

#ifdef DXX_BUILD_DESCENT_II
static int d2_write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return d2_write_weapon_order(path, wc->primary, wc->secondary);
}
#else
static int d1_write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return d1_write_weapon_order(path, wc->primary, wc->secondary);
}
#endif

extern "C" JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeAutoselectPatcher_nativeWriteAutoselect(
    JNIEnv *env, jclass, jstring jfilesDir,
    jintArray jprimary, jintArray jsecondary)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

#ifdef DXX_BUILD_DESCENT_II
	const char *subdir = "d2x-redux";
	const char *ext = ".plr";
	const int prim_len = D2_PRIMARY_ORDER_LEN;
	const int sec_len = D2_SECONDARY_ORDER_LEN;
#else
	const char *subdir = "d1x-redux";
	const char *ext = ".plx";
	const int prim_len = D1_PRIMARY_ORDER_LEN;
	const int sec_len = D1_SECONDARY_ORDER_LEN;
#endif

	jint *jprim = env->GetIntArrayElements(jprimary, NULL);
	jint *jsec = env->GetIntArrayElements(jsecondary, NULL);
	int jprim_len = env->GetArrayLength(jprimary);
	int jsec_len = env->GetArrayLength(jsecondary);

	unsigned char primary[D2_PRIMARY_ORDER_LEN];
	unsigned char secondary[D2_SECONDARY_ORDER_LEN];

	for (int i = 0; i < prim_len && i < jprim_len; i++)
		primary[i] = (unsigned char) (jprim[i] & 0xFF);
	for (int i = 0; i < sec_len && i < jsec_len; i++)
		secondary[i] = (unsigned char) (jsec[i] & 0xFF);

	env->ReleaseIntArrayElements(jprimary, jprim, JNI_ABORT);
	env->ReleaseIntArrayElements(jsecondary, jsec, JNI_ABORT);

	struct write_ctx wc = { primary, secondary };
	int total;
#ifdef DXX_BUILD_DESCENT_II
	total = for_each_pilot(files_dir, subdir, ext, d2_write_visitor, &wc);
#else
	total = for_each_pilot(files_dir, subdir, ext, d1_write_visitor, &wc);
#endif

	LOGI("nativeWriteAutoselect: patched %d file(s) in %s/%s", total, files_dir, subdir);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

/*
 * nativeGetWeaponNames() -> String[]
 * Returns primary names then secondary names concatenated.
 * D1: 5+1 primary (last = "Quad Lasers") + 5 secondary = 11 strings
 * D2: 10 primary + 10 secondary = 20 strings
 *
 * The index into this array matches the weapon index used in ordering arrays.
 * For D1, index 16 maps to the last primary entry (position 5).
 */
extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_NativeAutoselectPatcher_nativeGetWeaponNames(
    JNIEnv *env, jclass)
{
#ifdef DXX_BUILD_DESCENT_II
	const int total = 20;
#else
	const int total = 11;
#endif

	jclass strClass = env->FindClass("java/lang/String");
	jobjectArray result = env->NewObjectArray(total, strClass, NULL);
	int idx = 0;

#ifdef DXX_BUILD_DESCENT_II
	for (int i = 0; i < 10; i++)
		env->SetObjectArrayElement(result, idx++, env->NewStringUTF(d2_primary_names[i]));
	for (int i = 0; i < 10; i++)
		env->SetObjectArrayElement(result, idx++, env->NewStringUTF(d2_secondary_names[i]));
#else
	for (int i = 0; i < 5; i++)
		env->SetObjectArrayElement(result, idx++, env->NewStringUTF(d1_primary_names[i]));
	env->SetObjectArrayElement(result, idx++, env->NewStringUTF(D1_QUAD_LASERS_NAME));
	for (int i = 0; i < 5; i++)
		env->SetObjectArrayElement(result, idx++, env->NewStringUTF(d1_secondary_names[i]));
#endif

	return result;
}

/*
 * nativeGetDefaultAutoselect() -> int[]
 * Returns flat array: [primary..., secondary...]
 * Same format as nativeReadAutoselect.
 */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_NativeAutoselectPatcher_nativeGetDefaultAutoselect(
    JNIEnv *env, jclass)
{
#ifdef DXX_BUILD_DESCENT_II
	const int prim_len = D2_PRIMARY_ORDER_LEN;
	const int sec_len = D2_SECONDARY_ORDER_LEN;
	const unsigned char *dp = d2_default_primary;
	const unsigned char *ds = d2_default_secondary;
#else
	const int prim_len = D1_PRIMARY_ORDER_LEN;
	const int sec_len = D1_SECONDARY_ORDER_LEN;
	const unsigned char *dp = d1_default_primary;
	const unsigned char *ds = d1_default_secondary;
#endif

	int total = prim_len + sec_len;
	jintArray result = env->NewIntArray(total);
	jint flat[D2_PRIMARY_ORDER_LEN + D2_SECONDARY_ORDER_LEN];
	for (int i = 0; i < prim_len; i++) flat[i] = dp[i];
	for (int i = 0; i < sec_len; i++) flat[prim_len + i] = ds[i];
	env->SetIntArrayRegion(result, 0, total, flat);
	return result;
}

/*
 * nativeGetOrderLengths() -> int[]
 * Returns [primaryOrderLength, secondaryOrderLength]
 * D1: [7, 6], D2: [11, 11]
 */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_NativeAutoselectPatcher_nativeGetOrderLengths(
    JNIEnv *env, jclass)
{
#ifdef DXX_BUILD_DESCENT_II
	jint vals[] = { D2_PRIMARY_ORDER_LEN, D2_SECONDARY_ORDER_LEN };
#else
	jint vals[] = { D1_PRIMARY_ORDER_LEN, D1_SECONDARY_ORDER_LEN };
#endif
	jintArray result = env->NewIntArray(2);
	env->SetIntArrayRegion(result, 0, 2, vals);
	return result;
}

#endif /* ANDROID */
