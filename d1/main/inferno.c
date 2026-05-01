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
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
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

char copyright[] = "DESCENT   COPYRIGHT (C) 1994,1995 PARALLAX SOFTWARE CORPORATION";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#if defined(_WIN32) && defined(_MSC_VER)
#include <float.h>
#else
#include <fenv.h>
#endif
#include <SDL.h>

#ifdef __unix__
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
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
#include "segment.h"		//for Side_to_verts
#include "u_mem.h"
#include "screens.h"
#include "texmerge.h"
#include "menu.h"
#include "digi.h"
#include "palette.h"
#include "args.h"
#include "input_demo_rng_mode.h"
#include "titles.h"
#include "text.h"
#include "gauges.h"
#include "gamefont.h"
#include "kconfig.h"
#include "newmenu.h"
#include "config.h"
#include "multi.h"
#include "mission.h"
#include "songs.h"
#include "gameseq.h"
#include "playsave.h"
#include "state.h"
#include "collide.h"
#include "newdemo.h"
#include "input_demo_replay.h"
#include "joy.h"
#include "../texmap/scanline.h" //for select_tmap -MM
#include "event.h"
#include "rbaudio.h"
#include "messagebox.h"

#if defined(_WIN32) && defined(_MSC_VER)
static void configure_startup_fp_environment(void)
{
	unsigned int control_word = 0;

#if defined(_M_IX86)
	if (_controlfp_s(&control_word, _PC_53, _MCW_PC) != 0)
		Error("Failed to set floating point precision");
#endif
	if (_controlfp_s(&control_word, _RC_NEAR, _MCW_RC) != 0)
		Error("Failed to set floating point rounding mode");
	if (_controlfp_s(&control_word, 0, 0) != 0)
		Error("Failed to read floating point control word");
	if ((control_word & _MCW_RC) != _RC_NEAR)
		Error("Floating point rounding mode is not round-to-nearest");
#if defined(_M_IX86)
	if ((control_word & _MCW_PC) != _PC_53)
		Error("Floating point precision is not 53-bit");
#endif
}
#elif defined(FE_TONEAREST)
static void configure_startup_fp_environment(void)
{
	if (fesetround(FE_TONEAREST) != 0)
		Error("Failed to set floating point rounding mode");
	if (fegetround() != FE_TONEAREST)
		Error("Floating point rounding mode is not round-to-nearest");
}
#else
static void configure_startup_fp_environment(void)
{
}
#endif

#ifdef EDITOR
#include "editor/editor.h"
#include "editor/kdefs.h"
#include "ui.h"
#endif
#include "vers_id.h"
#ifdef USE_UDP
#include "net_udp.h"
#endif
#ifdef __ANDROID__
#include <android/log.h>
#include "android_crash_handler.h"
#define CHECKPOINT(msg) do { __android_log_print(ANDROID_LOG_INFO, "DXX", "INIT: " msg); crash_breadcrumb("INIT: " msg); } while(0)
#endif

int Screen_mode=-1;					//game screen or editor screen?
int descent_critical_error = 0;
unsigned int descent_critical_deverror = 0;
unsigned int descent_critical_errcode = 0;

int HiresGFXAvailable = 0;
int MacHog = 0;	// using a Mac hogfile?

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
	printf( "  -notitles                     Skip title screens\n");

	printf( "\n Controls:\n\n");
	printf( "  -nocursor                     Hide mouse cursor\n");
	printf( "  -nomouse                      Deactivate mouse\n");
	printf( "  -nojoystick                   Deactivate joystick\n");
	printf( "  -nostickykeys                 Make CapsLock and NumLock non-sticky\n");

	printf( "\n Sound:\n\n");
	printf( "  -nosound                      Disables sound output\n");
	printf( "  -nomusic                      Disables music output\n");
#ifdef    USE_SDLMIXER
	printf( "  -nosdlmixer                   Disable Sound output via SDL_mixer\n");
#endif // USE SDLMIXER

	printf( "\n Graphics:\n\n");
	printf( "  -lowresfont                   Force to use LowRes fonts\n");
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
	printf( "  -nobm                         Don't load BITMAPS.TBL and BITMAPS.BIN - use internal data\n");
#endif // EDITOR

	printf( "\n Debug (use only if you know what you're doing):\n\n");
	printf( "  -debug                        Enable debugging output.\n");
	printf( "  -verbose                      Enable verbose output.\n");
	printf( "  -safelog                      Write gamelog.txt unbuffered.\n\t\t\t\tUse to keep helpful output to trace program crashes.\n");
	printf( "  -norun                        Bail out after initialization\n");
	printf( "  -inputdemo-validate <s>       Validate input demo file rng_mode and exit\n");
	printf( "  -inputdemo-replay <s>         Replay input demo file through the D1 engine\n");
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

static unsigned int input_demo_replay_hash_u8_sequence(const ubyte *values, int count)
{
	unsigned int hash = 2166136261u;
	int i;

	for (i = 0; i < count; i++) {
		hash ^= values[i];
		hash *= 16777619u;
	}
	return hash;
}

static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return;
	PlayerCfg.AutoLeveling = player_cfg->auto_leveling;
	PlayerCfg.PersistentDebris = player_cfg->persistent_debris;
	PlayerCfg.NoFireAutoselect = player_cfg->no_fire_autoselect;
	PlayerCfg.CycleAutoselectOnly = player_cfg->cycle_autoselect_only;
	PlayerCfg.SelectAfterFire = player_cfg->select_after_fire;
	PlayerCfg.ClassicAutoselectWeapon = player_cfg->classic_autoselect_weapon;
	memcpy(PlayerCfg.PrimaryOrder, player_cfg->primary_order, MAX_PRIMARY_WEAPONS + 2);
	memcpy(PlayerCfg.SecondaryOrder, player_cfg->secondary_order, MAX_SECONDARY_WEAPONS + 1);
}

int input_demo_maybe_start_replay_from_cmdline(void)
{
	int arg_index = find_cmd_arg("-inputdemo-replay");
	int engine_mode;
	int demo_mode;
	const char *demo_path;
	const char *validation_error;
	char replay_error[256] = "";
	char mission_name[PATH_MAX] = "";
	char local_checkpoint_name[PATH_MAX] = "";
	const char *start_mode;
	const char *checkpoint_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	PHYSFS_file *checkpoint_file = NULL;
	const char *checkpoint_base_name;
	input_demo_player_cfg replay_player_cfg;
	char local_player_callsign[CALLSIGN_LEN + 1] = "";
	int have_replay_player_cfg = 0;

	if (!arg_index)
		return -1;
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-')
	{
		printf("Missing value for -inputdemo-replay\n");
		return 1;
	}
	demo_path = Args[arg_index + 1];
	engine_mode = d_rand_get_replay_mode();
	validation_error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&demo_mode);
	if (validation_error)
	{
		printf("Input demo replay file invalid: %s\n", demo_path);
		printf("%s\n", validation_error);
		printf("Active RNG backend expects: %s\n",
			input_demo_rng_mode_name(engine_mode));
		return 1;
	}
	if (!input_demo_replay_load(demo_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay load failed: %s\n", replay_error);
		return 1;
	}
	if (input_demo_replay_game() != INPUT_DEMO_GAME_D1)
	{
		printf("Input demo replay currently supports D1 demos only\n");
		input_demo_replay_unload();
		return 1;
	}
	if (Player_num >= 0 && Player_num < MAX_PLAYERS)
	{
		strncpy(local_player_callsign, Players[Player_num].callsign, CALLSIGN_LEN);
		local_player_callsign[CALLSIGN_LEN] = '\0';
	}
	have_replay_player_cfg = input_demo_replay_get_player_cfg(&replay_player_cfg);
	start_mode = input_demo_replay_start_mode();
	if (!start_mode)
	{
		printf("Input demo replay metadata is missing start_mode\n");
		input_demo_replay_unload();
		return 1;
	}
	if (!input_demo_replay_mission())
	{
		printf("Input demo replay metadata is missing mission\n");
		input_demo_replay_unload();
		return 1;
	}
	if (!d_stricmp(input_demo_replay_mission(), "d1"))
		snprintf(mission_name, sizeof(mission_name), "%s", D1_MISSION_FILENAME);
	else
		snprintf(mission_name, sizeof(mission_name), "%s", input_demo_replay_mission());
	if (!strcmp(start_mode, "new_level")) {
		if (!load_mission_by_name(mission_name))
		{
			printf("Input demo replay could not load mission: %s\n", mission_name);
			input_demo_replay_unload();
			return 1;
		}
		Difficulty_level = input_demo_replay_difficulty();
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		printf("Input demo replay starting: %s level %d, %u frames\n",
			mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
		input_demo_set_skip_level_intro(1);
		StartNewGame(input_demo_replay_level());
		return 0;
	}
	if (strcmp(start_mode, "save_checkpoint") != 0)
	{
		printf("Input demo replay start_mode not supported: %s\n", start_mode);
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_name = input_demo_replay_checkpoint_save_name();
	checkpoint_data = input_demo_replay_checkpoint_data();
	checkpoint_size = input_demo_replay_checkpoint_size();
	if (!input_demo_replay_has_checkpoint() || !checkpoint_name || !checkpoint_name[0] || !checkpoint_data || !checkpoint_size)
	{
		printf("Input demo replay is missing checkpoint data\n");
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_base_name = strrchr(checkpoint_name, '/');
	if (checkpoint_base_name)
		checkpoint_base_name++;
	else
		checkpoint_base_name = checkpoint_name;
	if (GameArg.SysUsePlayersDir)
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "Players/%s", checkpoint_base_name);
	else
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "%s", checkpoint_base_name);
	printf("Input demo replay checkpoint temp path: recorded=%s local=%s\n",
		checkpoint_name, local_checkpoint_name);
	if (!strncmp(local_checkpoint_name, "Players/", 8))
		PHYSFS_mkdir("Players");
	PHYSFS_delete(local_checkpoint_name);
	checkpoint_file = PHYSFS_openWrite(local_checkpoint_name);
	if (!checkpoint_file)
	{
		printf("Input demo replay could not write checkpoint file: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	if (PHYSFS_writeBytes(checkpoint_file, checkpoint_data, checkpoint_size) != (PHYSFS_sint64) checkpoint_size)
	{
		PHYSFS_close(checkpoint_file);
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not write checkpoint bytes: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_close(checkpoint_file);
	checkpoint_file = NULL;
	if (!state_restore_all_sub(local_checkpoint_name))
	{
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not restore checkpoint: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_delete(local_checkpoint_name);
	{
		int player_cfg_result;
		int replay_auto_level = -1;
		const char *replay_callsign;
		unsigned int primary_order_hash;
		unsigned int secondary_order_hash;
		fix player_mass = 0, player_drag = 0, player_brakes = 0;
		unsigned int player_phys_flags = 0;
		fix ship_mass = 0, ship_drag = 0, ship_brakes = 0;
		fix ship_max_thrust = 0, ship_max_rotthrust = 0, ship_wiggle = 0;

		if (ConsoleObject)
			replay_auto_level = (ConsoleObject->mtype.phys_info.flags & PF_LEVELLING) ? 1 : 0;
		if (!Players[Player_num].callsign[0] && local_player_callsign[0])
		{
			strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
			Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
		}
		if (Players[Player_num].callsign[0])
		{
			new_player_config();
			player_cfg_result = read_player_file();
		}
		else
			player_cfg_result = -1;
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		else if (replay_auto_level >= 0)
			PlayerCfg.AutoLeveling = replay_auto_level;
		primary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.PrimaryOrder, MAX_PRIMARY_WEAPONS + 2);
		secondary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1);
		replay_callsign = Players[Player_num].callsign[0] ? Players[Player_num].callsign : "<empty>";
		if (ConsoleObject) {
			player_mass = ConsoleObject->mtype.phys_info.mass;
			player_drag = ConsoleObject->mtype.phys_info.drag;
			player_brakes = ConsoleObject->mtype.phys_info.brakes;
			player_phys_flags = ConsoleObject->mtype.phys_info.flags;
		}
		if (Player_ship) {
			ship_mass = Player_ship->mass;
			ship_drag = Player_ship->drag;
			ship_brakes = Player_ship->brakes;
			ship_max_thrust = Player_ship->max_thrust;
			ship_max_rotthrust = Player_ship->max_rotthrust;
			ship_wiggle = Player_ship->wiggle;
		}
		con_printf(CON_NORMAL, "Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
			replay_callsign, player_cfg_result, PlayerCfg.AutoLeveling,
			PlayerCfg.PersistentDebris,
			PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
			PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
			primary_order_hash, secondary_order_hash,
			Players[Player_num].flags,
			player_mass, player_drag, player_brakes, player_phys_flags,
			ship_mass, ship_drag, ship_brakes, ship_max_thrust, ship_max_rotthrust, ship_wiggle);
	}
	if (d_stricmp(Current_mission_filename, mission_name) || Current_level_num != input_demo_replay_level() ||
		Difficulty_level != input_demo_replay_difficulty())
	{
		printf("Input demo replay checkpoint restore mismatch: mission=%s level=%d difficulty=%d\n",
			Current_mission_filename, Current_level_num, Difficulty_level);
		input_demo_replay_unload();
		return 1;
	}
	printf("Input demo replay starting: %s level %d, %u frames\n",
		mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
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
#if defined(WIN32) && (defined(_MSC_VER) || (defined(__clang__) && !defined(__i386__)))
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

//	DESCENT by Parallax Software
//		Descent Main

int main(int argc, char *argv[])
{
	mem_init();
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	configure_startup_fp_environment();
	PHYSFSX_init(argc, argv);
	con_init();  // Initialise the console

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

	if (! PHYSFSX_contfile_init("descent.hog", 1)) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s", PHYSFS_getWriteDir());
		size_t len = strlen(path);
		if (len >= 3 && (strcmp(path + len - 3, "\\.\\") == 0 || strcmp(path + len - 3, "/./") == 0))
			path[len - 3] = 0;
		else if (len && (path[len - 1] == '/' || path[len - 1] == '\\'))
			path[len - 1] = 0;
#define DXX_NAME_NUMBER	"1"
#define DXX_HOGFILE_NAMES	"descent.hog"
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

	switch (PHYSFSX_fsize("descent.hog"))
	{
		case D1_MAC_SHARE_MISSION_HOGSIZE:
		case D1_MAC_MISSION_HOGSIZE:
			MacHog = 1;	// used for fonts and the Automap
			break;
	}

	load_text();

	//print out the banner title
	con_printf(CON_NORMAL, "%s  %s\n", DESCENT_VERSION, g_descent_build_datetime); // D1X version
	con_printf(CON_NORMAL, "This is a MODIFIED version of Descent, based on %s.\n", BASED_VERSION);
	con_printf(CON_NORMAL, "%s\n%s\n",TXT_COPYRIGHT,TXT_TRADEMARK);
	con_printf(CON_NORMAL, "Copyright (C) 2005-2011 Christian Beckhaeuser\n\n");

	if (GameArg.DbgVerbose)
		con_printf(CON_VERBOSE,"%s%s", TXT_VERBOSE_1, "\n");
	
	ReadConfigFile();

	PHYSFSX_addArchiveContent();

	arch_init();

	select_tmap(GameArg.DbgTexMap);

	con_printf(CON_VERBOSE, "Going into graphics mode...\n");
	gr_set_mode(Game_screen_mode);

	// Load the palette stuff. Returns non-zero if error.
	con_printf(CON_DEBUG, "Initializing palette system...\n" );
	gr_use_palette_table( "PALETTE.256" );

	con_printf(CON_DEBUG, "Initializing font system...\n" );
	gamefont_init();	// must load after palette data loaded.

	set_default_handler(standard_handler);

	show_titles();

	set_screen_mode(SCREEN_MENU);

	con_printf( CON_DEBUG, "\nDoing gamedata_init..." );
	gamedata_init();

	if (GameArg.DbgNoRun)
		return(0);

	con_printf( CON_DEBUG, "\nInitializing texture caching system..." );
	texmerge_init( 10 );		// 10 cache bitmaps

	con_printf( CON_DEBUG, "\nRunning game...\n" );
#ifdef __ANDROID__
	CHECKPOINT("init_game");
#endif
	init_game();
#ifdef __ANDROID__
	CHECKPOINT("init_game done");
#endif

	Players[Player_num].callsign[0] = '\0';

	key_flush();

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


		{
			int replay_result = input_demo_maybe_start_replay_from_cmdline();

			if (replay_result > 0)
				return replay_result;
			if (replay_result < 0) {
				Game_mode = GM_GAME_OVER;
				DoMenu();
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
