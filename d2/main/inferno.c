/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * inferno.c: Entry point of program (main procedure)
 *
 * After main initializes everything, most of the time is spent in the loop
 * while (window_get_front())
 * In this loop, the main menu is brought up first.
 *
 * main() for Inferno
 *
 */

char copyright[] = "DESCENT II  COPYRIGHT (C) 1994-1996 PARALLAX SOFTWARE CORPORATION";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL.h>

#ifdef __unix__
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#include "android_crash_handler.h"
#define CHECKPOINT(msg) do { \
	__android_log_print(ANDROID_LOG_INFO, "DXX-Init", "CHECKPOINT: " msg); \
	crash_breadcrumb("INIT: " msg); \
} while(0)
#else
#define CHECKPOINT(msg) ((void)0)
#endif

#include "pstypes.h"
#include "strutil.h"
#include "console.h"
#include "gr.h"
#include "key.h"
#include "3d.h"
#include "bm.h"
#include "inferno.h"
#include "dxxerror.h"
#include "game.h"
#include "replay_debug_overlay.h"
#include "segment.h"		//for Side_to_verts
#include "u_mem.h"
#include "screens.h"
#include "texmap.h"
#include "texmerge.h"
#include "menu.h"
#include "digi.h"
#include "palette.h"
#include "args.h"
#include "mission.h"
#include "maths.h"
#include "input_demo_replay.h"
#include "input_demo_rng_trace.h"
#include "input_demo_state_trace.h"
#include "input_demo_rng_mode.h"
#include "input_demo_debug_logging.h"
#include "titles.h"
#include "text.h"
#include "gauges.h"
#include "gamefont.h"
#include "kconfig.h"
#include "newmenu.h"
#include "config.h"
#include "multi.h"
#include "songs.h"
#include "gameseq.h"
#include "gamepal.h"
#include "movie.h"
#include "playsave.h"
#include "state.h"
#include "input_demo_start.h"
#include "collide.h"
#include "newdemo.h"
#include "joy.h"
#include "input_demo_fp_env.h"
#include "../texmap/scanline.h" //for select_tmap -MM
#include "event.h"
#include "rbaudio.h"
#include "messagebox.h"

static void configure_startup_fp_environment(void)
{
	char error[128] = "";

	if (!input_demo_configure_startup_fp_environment(error, sizeof(error)))
		Error("%s", error);
}

#ifdef EDITOR
#include "editor/editor.h"
#include "editor/kdefs.h"
#include "ui.h"
#endif
#include "vers_id.h"
#ifdef USE_UDP
#include "net_udp.h"
#endif

//Current version number

int Screen_mode=-1;					//game screen or editor screen?
int descent_critical_error = 0;
unsigned int descent_critical_deverror = 0;
unsigned int descent_critical_errcode = 0;

#ifdef	EDITOR
char	Auto_file[128] = "";
#endif

extern void piggy_init_pigfile(char *filename);
extern void arch_init(void);

//read help from a file & print to screen
void print_commandline_help()
{
	printf( "\n System Options:\n\n");
	printf( "  -nonicefps                    Don't free CPU-cycles\n");
	printf( "  -maxfps <n>                   Set maximum framerate to <n>\n\t\t\t\t(default: %i, availble: 1-%i)\n", MAXIMUM_FPS, MAXIMUM_FPS);
	printf( "  -hogdir <s>                   set shared data directory to <s>\n");
	printf( "  -nohogdir                     don't try to use shared data directory\n");
	printf( "  -use_players_dir              put player files and saved games in Players subdirectory\n");
	printf( "  -lowmem                       Lowers animation detail for better performance with\n\t\t\t\tlow memory\n");
	printf( "  -pilot <s>                    Select pilot <s> automatically\n");
	printf( "  -autodemo                     Start in demo mode\n");
	printf( "  -window                       Run the game in a window\n");
	printf( "  -noborders                    Do not show borders in window mode\n");
	printf( "  -nomovies                     Don't play movies\n");

	printf( "\n Controls:\n\n");
	printf( "  -nocursor                     Hide mouse cursor\n");
	printf( "  -nomouse                      Deactivate mouse\n");
	printf( "  -nojoystick                   Deactivate joystick\n");
	printf( "  -nostickykeys                 Make CapsLock and NumLock non-sticky\n");

	printf( "\n Sound:\n\n");
	printf( "  -nosound                      Disables sound output\n");
	printf( "  -nomusic                      Disables music output\n");
	printf( "  -sound11k                     Use 11KHz sounds\n");
#ifdef    USE_SDLMIXER
	printf( "  -nosdlmixer                   Disable Sound output via SDL_mixer\n");
#endif // USE SDLMIXER

	printf( "\n Graphics:\n\n");
	printf( "  -lowresfont                   Force to use LowRes fonts\n");
	printf( "  -lowresgraphics               Force to use LowRes graphics\n");
	printf( "  -lowresmovies                 Play low resolution movies if available (for slow machines)\n");
#ifdef    OGL
	printf( "  -gl_fixedfont                 Do not scale fonts to current resolution\n");
#endif // OGL

#if defined(USE_UDP)
	printf( "\n Multiplayer:\n\n");
	printf( "  -udp_hostaddr <s>             Use IP address/Hostname <s> for manual game joining\n\t\t\t\t(default: %s)\n", UDP_MANUAL_ADDR_DEFAULT);
	printf( "  -udp_hostport <n>             Use UDP port <n> for manual game joining (default: %i)\n", UDP_PORT_DEFAULT);
	printf( "  -udp_myport <n>               Set my own UDP port to <n> (default: %i)\n", UDP_PORT_DEFAULT);
#ifdef USE_TRACKER
	printf( "  -tracker_hostaddr <n>         Address of Tracker server to register/query games to/from\n\t\t\t\t(default: %s)\n", TRACKER_ADDR_DEFAULT);
	printf( "  -tracker_hostport <n>         Port of Tracker server to register/query games to/from\n\t\t\t\t(default: %i)\n", TRACKER_PORT_DEFAULT);
#endif // USE_TRACKER
#endif // defined(USE_UDP)

#ifdef    EDITOR
	printf( "\n Editor:\n\n");
	printf( "  -autoload <s>                 Autoload level <s> in the editor\n");
	printf( "  -macdata                      Read and write mac data files in editor (swap colors)\n");
	printf( "  -hoarddata                    Make the hoard ham file from some files, then exit\n");
#endif // EDITOR

	printf( "\n Debug (use only if you know what you're doing):\n\n");
	printf( "  -debug                        Enable debugging output.\n");
	printf( "  -verbose                      Enable verbose output.\n");
	printf( "  -safelog                      Write gamelog.txt unbuffered.\n\t\t\t\tUse to keep helpful output to trace program crashes.\n");
	printf( "  -norun                        Bail out after initialization\n");
	printf( "  -inputdemo-validate <s>       Validate input demo file rng_mode and exit\n");
	printf( "  -inputdemo-replay <s>         Replay input demo file through the D2 engine\n");
	printf( "  -inputdemo-actual-result <s>  Write replay final-result JSON to <s>\n");
	printf( "  -inputdemo-replay-labels      Draw replay robot labels and frame counter during replay\n");
#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE
	printf( "  -inputdemo-debug-log          Enable optional input demo debug probe logging\n");
#endif
	printf( "  -inputdemo-state-log <s>      Write replay frame-state JSONL during input demo replay\n");
	printf( "  -inputdemo-rng-trace <s>      Write replay RNG trace JSONL during input demo replay\n");
	printf( "  -classicdemo-dump-json <s> <s> Dump classic .dem to JSONL and exit\n");
	printf( "  -renderstats                  Enable renderstats info by default\n");
	printf( "  -text <s>                     Specify alternate .tex file\n");
	printf( "  -tmap <s>                     Select texmapper <s> to use\n\t\t\t\t(default: c, available: c, fp, quad, i386)\n");
	printf( "  -showmeminfo                  Show memory statistics\n");
	printf( "  -nodoublebuffer               Disable Doublebuffering\n");
	printf( "  -bigpig                       Use uncompressed RLE bitmaps\n");
	printf( "  -16bpp                        Use 16Bpp instead of 32Bpp\n");
#ifdef    OGL
	printf( "  -gl_oldtexmerge               Use old texmerge, uses more ram, but might be faster\n");
	printf( "  -gl_intensity4_ok <n>         Override DbgGlIntensity4Ok (default: 1)\n");
	printf( "  -gl_luminance4_alpha4_ok <n>  Override DbgGlLuminance4Alpha4Ok (default: 1)\n");
	printf( "  -gl_rgba2_ok <n>              Override DbgGlRGBA2Ok (default: 1)\n");
	printf( "  -gl_readpixels_ok <n>         Override DbgGlReadPixelsOk (default: 1)\n");
	printf( "  -gl_gettexlevelparam_ok <n>   Override DbgGlGetTexLevelParamOk (default: 1)\n");
#else
	printf( "  -hwsurface                    Use SDL HW Surface\n");
	printf( "  -asyncblit                    Use queued blits over SDL. Can speed up rendering\n");
#endif // OGL

	printf( "\n Help:\n\n");
	printf( "  -help, -h, -?, ?             View this help screen\n");
	printf( "\n\n");
}

static int find_cmd_arg(const char *name)
{
	int i;

	for (i = 1; i < Num_args; ++i)
		if (!d_stricmp(Args[i], name))
			return i;

	return 0;
}

static int maybe_validate_input_demo_metadata(void)
{
	int arg_index = find_cmd_arg("-inputdemo-validate");
	int engine_mode;
	int demo_mode;
	const char *demo_path;
	const char *error;

	if (!arg_index)
		return -1;
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-')
	{
		printf("Missing value for -inputdemo-validate\n");
		return 1;
	}
	demo_path = Args[arg_index + 1];
	engine_mode = d_rand_get_replay_mode();
	error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&demo_mode);
	if (error)
	{
		printf("Input demo file invalid: %s\n", demo_path);
		printf("%s\n", error);
		printf("Active RNG backend expects: %s\n",
			input_demo_rng_mode_name(engine_mode));
		return 1;
	}
	printf("Input demo file OK: %s\n", demo_path);
	printf("rng_mode: %s\n", input_demo_rng_mode_name(demo_mode));
	return 0;
}

int input_demo_maybe_start_replay_from_cmdline(void)
{
	int arg_index = find_cmd_arg("-inputdemo-replay");
	int actual_result_arg_index = find_cmd_arg("-inputdemo-actual-result");
	int replay_labels_arg_index = find_cmd_arg("-inputdemo-replay-labels");
	int debug_log_arg_index = 0;
	#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE
	debug_log_arg_index = find_cmd_arg("-inputdemo-debug-log");
	#endif
	int state_log_arg_index = find_cmd_arg("-inputdemo-state-log");
	int rng_trace_arg_index = find_cmd_arg("-inputdemo-rng-trace");
	const char *demo_path;
	const char *actual_result_path = NULL;
	const char *state_log_path = NULL;
	const char *rng_trace_path = NULL;
	char replay_error[256] = "";

	input_demo_debug_set_enabled(debug_log_arg_index ? 1 : 0);
	g_replay_robot_labels_enabled = replay_labels_arg_index ? 1 : 0;

	if (!arg_index)
		return -1;
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-')
	{
		printf("Missing value for -inputdemo-replay\n");
		return 1;
	}
	if (actual_result_arg_index)
	{
		if (actual_result_arg_index + 1 >= Num_args || !Args[actual_result_arg_index + 1] || Args[actual_result_arg_index + 1][0] == '-')
		{
			printf("Missing value for -inputdemo-actual-result\n");
			return 1;
		}
		actual_result_path = Args[actual_result_arg_index + 1];
	}
	if (state_log_arg_index)
	{
		if (state_log_arg_index + 1 >= Num_args || !Args[state_log_arg_index + 1] || Args[state_log_arg_index + 1][0] == '-')
		{
			printf("Missing value for -inputdemo-state-log\n");
			return 1;
		}
		state_log_path = Args[state_log_arg_index + 1];
	}
	if (rng_trace_arg_index)
	{
		if (rng_trace_arg_index + 1 >= Num_args || !Args[rng_trace_arg_index + 1] || Args[rng_trace_arg_index + 1][0] == '-')
		{
			printf("Missing value for -inputdemo-rng-trace\n");
			return 1;
		}
		rng_trace_path = Args[rng_trace_arg_index + 1];
	}
	demo_path = Args[arg_index + 1];
	if (!input_demo_load_replay_from_path(demo_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay load failed: %s\n", replay_error);
		return 1;
	}
	if (actual_result_path)
		input_demo_replay_set_actual_result_path(actual_result_path);
	if (rng_trace_path && !input_demo_rng_trace_start_replay(rng_trace_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay rng trace start failed: %s\n", replay_error);
		input_demo_replay_unload();
		return 1;
	}
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay state trace start failed: %s\n", replay_error);
		input_demo_replay_unload();
		return 1;
	}
	if (rng_trace_path)
		printf("Input demo replay rng trace: %s\n", rng_trace_path);
	if (state_log_path)
		printf("Input demo replay state trace: %s\n", state_log_path);
	if (actual_result_path)
		printf("Input demo replay actual result: %s\n", actual_result_path);
	return input_demo_start_loaded_replay();
}

static int maybe_dump_classic_demo_json(void)
{
	int arg_index = find_cmd_arg("-classicdemo-dump-json");
	const char *demo_path;
	const char *output_path;
	char dump_error[256] = "";

	if (!arg_index)
		return -1;
	if (arg_index + 2 >= Num_args || !Args[arg_index + 1] || !Args[arg_index + 2] ||
		Args[arg_index + 1][0] == '-' || Args[arg_index + 2][0] == '-')
	{
		printf("Usage: -classicdemo-dump-json <demo.dem> <output.jsonl>\n");
		return 1;
	}
	demo_path = Args[arg_index + 1];
	output_path = Args[arg_index + 2];
	if (!newdemo_dump_json(demo_path, output_path, dump_error, sizeof(dump_error)))
	{
		printf("Classic demo JSON dump failed: %s\n", dump_error[0] ? dump_error : "unknown error");
		return 1;
	}
	printf("Classic demo JSON dump written: %s\n", output_path);
	return 0;
}

int Quitting = 0;

// Default event handler for everything except the editor
int standard_handler(d_event *event)
{
	int key;

	if (Quitting)
	{
		window *wind = window_get_front();
		if (!wind)
			return 0;
	
		if (wind == Game_wind)
		{
#ifdef ANDROID
			/* android port: EXIT button sets force_quit to skip
			 * the confirmation dialog and exit immediately */
			extern volatile int android_force_quit;
			if (android_force_quit) {
				GameArg.SysAutoDemo = 0;
			} else
#endif
			{
				int choice;
				Quitting = 0;
				choice=nm_messagebox( NULL, 2, TXT_YES, TXT_NO, TXT_ABORT_GAME );
				if (choice != 0)
					return 0;
				else
				{
					GameArg.SysAutoDemo = 0;
					Quitting = 1;
				}
			}
		}
		
		// Close front window, let the code flow continue until all windows closed or quit cancelled
		if (!window_close(wind))
			Quitting = 0;
		
		return 1;
	}

	switch (event->type)
	{
		case EVENT_MOUSE_BUTTON_DOWN:
		case EVENT_MOUSE_BUTTON_UP:
			// No window selecting
			// We stay with the current one until it's closed/hidden or another one is made
			// Not the case for the editor
			break;

		case EVENT_KEY_COMMAND:
			key = event_key_get(event);

			switch (key)
			{
#ifdef macintosh
				case KEY_COMMAND + KEY_SHIFTED + KEY_3:
#endif
				case KEY_PRINT_SCREEN:
				{
					gr_set_current_canvas(NULL);
					save_screen_shot(0);
					return 1;
				}

				case KEY_ALTED+KEY_ENTER:
				case KEY_ALTED+KEY_PADENTER:
					if (Game_wind)
						if (Game_wind == window_get_front())
							return 0;
					gr_toggle_fullscreen();
					return 1;

#ifndef NDEBUG
				case KEY_BACKSP:
					Int3();
					return 1;
#endif

#if defined(__APPLE__) || defined(macintosh)
				case KEY_COMMAND+KEY_Q:
					// Alt-F4 already taken, too bad
					Quitting = 1;
					return 1;
#endif
				case KEY_SHIFTED + KEY_ESC:
					con_showup();
					return 1;
			}
			break;

		case EVENT_WINDOW_DRAW:
		case EVENT_IDLE:
			//see if redbook song needs to be restarted
			RBACheckFinishedHook();
			return 1;

		case EVENT_QUIT:
#ifdef EDITOR
			if (SafetyCheck())
#endif
				Quitting = 1;
			return 1;

		default:
			break;
	}

	return 0;
}

// Use SEH for catching exceptions on Windows, this is needed because of the SDL parachute
// But only on MSVC/64-bit clang (SEH is broken on 32-bit clang https://github.com/llvm/llvm-project/issues/25753)
#if defined(WIN32) && (!defined(__clang__) || !defined(__i386__))
int inner_main(int argc, char *argv[]);
LONG WINAPI win32_exception_handler(EXCEPTION_POINTERS* exceptionPointers);

int main(int argc, char *argv[])
{
	__try {
		return inner_main(argc, argv);
	} __except (win32_exception_handler(GetExceptionInformation())) {
		return 1;
	}
}

#undef main
#define main inner_main
#endif

jmp_buf LeaveEvents;
#define PROGNAME argv[0]

//	DESCENT II by Parallax Software
//		Descent Main

int main(int argc, char *argv[])
{
	CHECKPOINT("main() entered");
	mem_init();
	CHECKPOINT("mem_init done");
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	CHECKPOINT("error_init done");
	configure_startup_fp_environment();
	CHECKPOINT("fp environment configured");
	PHYSFSX_init(argc, argv);
	CHECKPOINT("PHYSFSX_init done");
	con_init();  // Initialise the console
	CHECKPOINT("con_init done");

	setbuf(stdout, NULL); // unbuffered output via printf
#ifdef _WIN32
	freopen( "CON", "w", stdout );
	freopen( "CON", "w", stderr );
#endif

	if (GameArg.SysShowCmdHelp) {
		print_commandline_help();

		return(0);
	}
	{
		int validate_result = maybe_validate_input_demo_metadata();
		if (validate_result >= 0)
			return validate_result;
	}

	printf("\nType %s -help' for a list of command-line options.\n\n", PROGNAME);

	PHYSFSX_listSearchPathContent();
	
	if (!PHYSFSX_checkSupportedArchiveTypes())
		return(0);

	CHECKPOINT("searching for hog files");

	if (! PHYSFSX_contfile_init("descent2.hog", 1) &&
		! PHYSFSX_contfile_init("d2demo.hog", 1)) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s", PHYSFS_getWriteDir());
		size_t len = strlen(path);
		if (len >= 3 && (strcmp(path + len - 3, "\\.\\") == 0 || strcmp(path + len - 3, "/./") == 0))
			path[len - 3] = 0;
		else if (len && (path[len - 1] == '/' || path[len - 1] == '\\'))
			path[len - 1] = 0;
#define DXX_NAME_NUMBER	"2"
#define DXX_HOGFILE_NAMES	"descent2.hog or d2demo.hog"
#if defined(__APPLE__)
#define DXX_HOGFILE_PROGRAM_DATA_DIRECTORY	\
			      "\t%s\n" \
			      "\tDirectory containing D" DXX_NAME_NUMBER "X-Redux\n"
#elif defined(__unix__)
#define DXX_HOGFILE_PROGRAM_DATA_DIRECTORY	\
			      "\t%s\n"	\
			      "\t" SHAREPATH "\n"
#else // Windows
#define DXX_HOGFILE_PROGRAM_DATA_DIRECTORY	\
			      "\t%s\n"
#endif
#if (defined(__APPLE__) && defined(__MACH__)) || defined(macintosh)
#define DXX_HOGFILE_APPLICATION_BUNDLE	\
				  "\tIn 'Resources' inside the application bundle\n"
#else
#define DXX_HOGFILE_APPLICATION_BUNDLE	""
#endif
#define DXX_MISSING_HOGFILE_ERROR_TEXT	\
		"Could not find a valid hog file (" DXX_HOGFILE_NAMES ")\nPossible locations are:\n"	\
		DXX_HOGFILE_PROGRAM_DATA_DIRECTORY	\
		"\tIn a subdirectory called 'Data'\n"	\
		DXX_HOGFILE_APPLICATION_BUNDLE	\
		"Or use the -hogdir option to specify an alternate location."
		Error(DXX_MISSING_HOGFILE_ERROR_TEXT, path);
	}

	CHECKPOINT("hog files loaded");
	load_text();

	//print out the banner title
	con_printf(CON_NORMAL, "%s%s  %s\n", DESCENT_VERSION, PHYSFSX_exists(MISSION_DIR "d2x.hog",1) ? "  Vertigo Enhanced" : "", g_descent_build_datetime); // D2X version
	con_printf(CON_NORMAL, "This is a MODIFIED version of Descent 2, based on %s.\n", BASED_VERSION);
	con_printf(CON_NORMAL, "%s\n%s\n",TXT_COPYRIGHT,TXT_TRADEMARK);
	con_printf(CON_NORMAL, "Copyright (C) 1999 Peter Hawkins, 2002 Bradley Bell, 2005-2011 Christian Beckhaeuser\n\n");

	if (GameArg.DbgVerbose)
		con_printf(CON_VERBOSE,"%s%s", TXT_VERBOSE_1, "\n");
	
	ReadConfigFile();
	CHECKPOINT("config read");

	PHYSFSX_addArchiveContent();
	if (find_cmd_arg("-classicdemo-dump-json")) {
		gr_use_palette_table(D2_DEFAULT_PALETTE);
		gamedata_init();
		texmerge_init(10);
		piggy_init_pigfile("groupa.pig");
		init_game();
		return maybe_dump_classic_demo_json();
	}

	CHECKPOINT("calling arch_init");
	arch_init();
	CHECKPOINT("arch_init done");

	select_tmap(GameArg.DbgTexMap);

	Lighting_on = 1;

	con_printf(CON_VERBOSE, "Going into graphics mode...\n");
	CHECKPOINT("calling gr_set_mode");
	gr_set_mode(Game_screen_mode);
	CHECKPOINT("gr_set_mode done");

	// Load the palette stuff. Returns non-zero if error.
	con_printf(CON_DEBUG, "Initializing palette system...\n" );
	gr_use_palette_table(D2_DEFAULT_PALETTE );
	CHECKPOINT("palette loaded");

	con_printf(CON_DEBUG, "Initializing font system...\n" );
	gamefont_init();	// must load after palette data loaded.
	CHECKPOINT("fonts loaded");

	set_default_handler(standard_handler);

	con_printf( CON_DEBUG, "Initializing movie libraries...\n" );
	init_movies();		//init movie libraries
	CHECKPOINT("movies init done");

	show_titles();
	CHECKPOINT("titles shown");

	set_screen_mode(SCREEN_MENU);
	CHECKPOINT("screen mode set to menu");

	con_printf( CON_DEBUG, "\nDoing gamedata_init..." );
	gamedata_init();
	CHECKPOINT("gamedata_init done");

	#ifdef EDITOR
	if (GameArg.EdiSaveHoardData) {
		save_hoard_data();
		exit(1);
	}
	#endif

	if (GameArg.DbgNoRun)
		return(0);

	con_printf( CON_DEBUG, "\nInitializing texture caching system..." );
	texmerge_init( 10 );		// 10 cache bitmaps

	CHECKPOINT("piggy_init_pigfile start");
	piggy_init_pigfile("groupa.pig");	//get correct pigfile
	CHECKPOINT("piggy_init_pigfile done");

	con_printf( CON_DEBUG, "\nRunning game...\n" );
	CHECKPOINT("init_game start");
	init_game();
	CHECKPOINT("init_game done");

	Players[Player_num].callsign[0] = '\0';

	//	If built with editor, option to auto-load a level and quit game
	//	to write certain data.
	#ifdef	EDITOR
	if (GameArg.EdiAutoLoad) {
		strcpy(Auto_file, GameArg.EdiAutoLoad);
		strcpy(Players[0].callsign, "dummy");
	} else
	#endif
	{
		if(GameArg.SysPilot)
		{
			char filename[32] = "";
			int j;

			if (GameArg.SysUsePlayersDir)
				strcpy(filename, "Players/");
			strncat(filename, GameArg.SysPilot, 12);
			filename[8 + 12] = '\0';	// unfortunately strncat doesn't put the terminating 0 on the end if it reaches 'n'
			for (j = GameArg.SysUsePlayersDir? 8 : 0; filename[j] != '\0'; j++) {
				switch (filename[j]) {
					case ' ':
						filename[j] = '\0';
				}
			}
			if(!strstr(filename,".plr")) // if player hasn't specified .plr extension in argument, add it
				strcat(filename,".plr");
			if(PHYSFSX_exists(filename,0))
			{
				strcpy(strstr(filename,".plr"),"\0");
				strcpy(Players[Player_num].callsign, GameArg.SysUsePlayersDir? &filename[8] : filename);
				read_player_file();
				WriteConfigFile();
			}
		}
	}

#ifdef EDITOR
	if (GameArg.EdiAutoLoad) {
		strcpy((char *)&Level_names[0], Auto_file);
		LoadLevel(1, 1);
	}
	else
#endif
	{
		int replay_result = input_demo_maybe_start_replay_from_cmdline();

		if (replay_result > 0)
			return replay_result;
		if (replay_result < 0) {
			Game_mode = GM_GAME_OVER;
			CHECKPOINT("entering DoMenu");
			DoMenu();
			CHECKPOINT("DoMenu returned");
		}
	}

	setjmp(LeaveEvents);
	while (window_get_front())
		// Send events to windows and the default handler
		event_process();
	
	// Tidy up - avoids a crash on exit
	{
		window *wind;

		show_menus();
		while ((wind = window_get_front()))
			window_close(wind);
	}

	WriteConfigFile();
	show_order_form();

	con_printf( CON_DEBUG, "\nCleanup...\n" );
	close_game();
	texmerge_close();
	gamedata_close();
	gamefont_close();
	free_text();
	args_exit();
	newmenu_free_background();
	free_mission();
	PHYSFSX_removeArchiveContent();
	reset_observatory_stats();

	return(0);		//presumably successful exit
}
