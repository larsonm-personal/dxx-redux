#include <stdio.h>
#include <string.h>

#include "args.h"
#ifdef DXX_ARGS_TEST_D2
#include "digi.h"
#endif
#include "game.h"
#ifdef USE_UDP
#include "net_udp.h"
#endif

#define CHECK(condition, message)                   \
	do {                                            \
		if (!(condition)) {                         \
			fprintf(stderr, "FAIL: %s\n", message); \
			return 0;                               \
		}                                           \
	} while (0)

void con_printf(int level, const char *fmt, ...)
{
	(void) level;
	(void) fmt;
}

static int expect_default_table(int android_mode)
{
	struct Arg actual;
	struct Arg expected;

	memcpy(&actual, &GameArg, sizeof(actual));
	memset(&expected, 0, sizeof(expected));
	expected.SysUseNiceFPS = 1;
	expected.SysMaxFPS = MAXIMUM_FPS;
	expected.SysUsePlayersDir = android_mode;
#ifndef USE_SDLMIXER
	expected.SndDisableSdlMixer = 1;
#endif
#ifdef DXX_ARGS_TEST_D2
	expected.SndDigiSampleRate = SAMPLE_RATE_22K;
	expected.GfxMovieHires = 1;
	expected.GfxHiresGFXAvailable = 1;
#endif
	expected.GfxHiresFNTAvailable = 1;
#ifdef USE_UDP
	CHECK(actual.MplUdpHostAddr != NULL &&
	          !strcmp(actual.MplUdpHostAddr, UDP_MANUAL_ADDR_DEFAULT),
	      "UDP host default");
	actual.MplUdpHostAddr = NULL;
#ifdef USE_TRACKER
	CHECK(actual.MplTrackerAddr != NULL &&
	          !strcmp(actual.MplTrackerAddr, TRACKER_ADDR_DEFAULT),
	      "tracker host default");
	actual.MplTrackerAddr = NULL;
	expected.MplTrackerPort = TRACKER_PORT_DEFAULT;
#endif
#endif
	expected.DbgUseDoubleBuffer = 1;
	expected.DbgBigPig = 1;
	expected.DbgBpp = 32;
#ifdef OGL
	expected.DbgAltTexMerge = 1;
	expected.DbgGlIntensity4Ok = 1;
	expected.DbgGlLuminance4Alpha4Ok = 1;
	expected.DbgGlRGBA2Ok = 1;
	expected.DbgGlReadPixelsOk = 1;
	expected.DbgGlGetTexLevelParamOk = 1;
#endif
	expected.LogNetTraffic = android_mode;

	CHECK(!memcmp(&actual, &expected, sizeof(expected)),
	      "complete default field table");
	return 1;
}

static int find_preserved_arg(const char *name, const char *value)
{
	int i;

	for (i = 1; i + 1 < Num_args; i++)
		if (!strcmp(Args[i], name) && !strcmp(Args[i + 1], value))
			return 1;
	return 0;
}

static int test_android_defaults(void)
{
	char *argv[] = { "ignored" };

	memset(&GameArg, 0xa5, sizeof(GameArg));
	InitArgsAndroid(1, argv);
	CHECK(Num_args == 1 && !strcmp(Args[0], "android"),
	      "Android argv sentinel");
	CHECK(expect_default_table(1), "Android default table");
	args_exit();
	return 1;
}

static int test_android_supported_overrides(void)
{
	char *argv[] = {
		"ignored",
		"-PILOT",
		"MixedPilot",
		"-resume-save",
		"Players/Mixed.sg0",
		"-inputdemo-replay",
		"Replay/Mixed.dximdemo",
		"-nonicefps",
		"-maxfps",
		"144",
		"-nosound",
		"-nomusic",
		"-inputdemo-norender",
		"-lowresfont",
#ifdef DXX_ARGS_TEST_D2
		"-lowresgraphics",
		"-lowresmovies",
		"-sound11k",
#endif
#ifdef OGL
		"-gl_oldtexmerge",
#endif
	};
	const int argc = (int) (sizeof(argv) / sizeof(argv[0]));

	memset(&GameArg, 0xa5, sizeof(GameArg));
	InitArgsAndroid(argc, argv);
	CHECK(Num_args == argc, "Android argv count");
	CHECK(!strcmp(Args[1], "-pilot"), "Android option names lowercase");
	CHECK(GameArg.SysPilot != NULL &&
	          !strcmp(GameArg.SysPilot, "MixedPilot"),
	      "pilot override preserves value case");
	CHECK(find_preserved_arg("-resume-save", "Players/Mixed.sg0"),
	      "resume argument preserved");
	CHECK(find_preserved_arg("-inputdemo-replay",
	                         "Replay/Mixed.dximdemo"),
	      "replay argument preserved");
	CHECK(!GameArg.SysUseNiceFPS, "nonicefps override");
	CHECK(GameArg.SysMaxFPS == 144, "maxfps override");
	CHECK(GameArg.SysUsePlayersDir, "Android player directory invariant");
	CHECK(GameArg.SndNoSound && GameArg.SndNoMusic,
	      "Android audio disable overrides");
	CHECK(GameArg.SysInputDemoNoRender, "no-render override");
	CHECK(!GameArg.GfxHiresFNTAvailable, "low-resolution font override");
	CHECK(GameArg.LogNetTraffic, "Android native network logging gate");
#ifdef DXX_ARGS_TEST_D2
	CHECK(!GameArg.GfxHiresGFXAvailable,
	      "D2 low-resolution graphics override");
	CHECK(!GameArg.GfxMovieHires, "D2 low-resolution movie override");
	CHECK(GameArg.SndDigiSampleRate == SAMPLE_RATE_22K,
	      "D2 Android sample-rate invariant");
#endif
#ifdef OGL
	CHECK(GameArg.DbgAltTexMerge,
	      "Android texture-merge invariant");
#endif
	args_exit();
	return 1;
}

static int test_desktop_defaults_and_network_override(void)
{
	memset(&GameArg, 0xa5, sizeof(GameArg));
	Num_args = 0;
	ReadCmdArgs();
	CHECK(expect_default_table(0), "desktop default table");

	memset(&GameArg, 0xa5, sizeof(GameArg));
	Num_args = 2;
	Args[0] = "test";
	Args[1] = "-netlog";
	ReadCmdArgs();
	CHECK(GameArg.LogNetTraffic, "desktop network log override");
	Num_args = 0;
	return 1;
}

int main(void)
{
	if (!test_android_defaults() ||
	    !test_android_supported_overrides() ||
	    !test_desktop_defaults_and_network_override())
		return 1;
#ifdef DXX_ARGS_TEST_D2
	puts("PASS: D2 desktop and Android argument defaults are explicit");
#else
	puts("PASS: D1 desktop and Android argument defaults are explicit");
#endif
	return 0;
}
