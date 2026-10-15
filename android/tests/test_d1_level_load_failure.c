/* Exercise the real D1 LoadLevel caller without assets or graphics initialization */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "gameseq.h"
#include "mission.h"

static jmp_buf boundary;
static int load_result, load_calls, rescan_calls;
static char diagnostic[256];

enum { FATAL_BOUNDARY = 1,
	   SETUP_BOUNDARY = 2 };

int __wrap_load_level(const char *filename)
{
	if (strcmp(filename, "failure.rdl")) abort();
	++load_calls;
	return load_result;
}

void __wrap_Error(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(diagnostic, sizeof(diagnostic), format, args);
	va_end(args);
	longjmp(boundary, FATAL_BOUNDARY);
}

void __wrap_secret_area_rescan_current_level(void)
{
	++rescan_calls;
}

void __wrap_gr_use_palette_table(char *filename)
{
	if (strcmp(filename, "palette.256")) abort();
	longjmp(boundary, SETUP_BOUNDARY);
}

static int exercise(int result)
{
	load_result = result;
	load_calls = rescan_calls = 0;
	diagnostic[0] = 0;
	Current_level_num = 0;
	const int reached = setjmp(boundary);
	if (!reached) {
		LoadLevel(1, 0);
		fprintf(stderr, "LoadLevel unexpectedly returned\n");
		return 1;
	}
	if (load_calls != 1) return 1;
	if (!result) {
		if (reached != SETUP_BOUNDARY || Current_level_num != 1 || rescan_calls != 1 || diagnostic[0]) {
			fprintf(stderr, "Valid level did not retain normal commit and setup ordering\n");
			return 1;
		}
	} else {
		if (reached != FATAL_BOUNDARY || Current_level_num != 0 || rescan_calls != 0 ||
		    strcmp(diagnostic, "Couldn't load level file <failure.rdl>, error = 3")) {
			fprintf(stderr, "Failed level continued into setup: boundary=%d current=%d rescan=%d diagnostic=%s\n",
			        reached, Current_level_num, rescan_calls, diagnostic);
			return 1;
		}
	}
	return 0;
}

int main(void)
{
	Mission mission = { 0 };
	d_fname levels[1] = { "failure.rdl" };
	Current_mission = &mission;
	mission.level_names = levels;
	mission.last_level = 1;
	Player_num = 0;
	const int failed = exercise(3) | exercise(0);
	Current_mission = NULL;
	if (!failed) puts("PASS: real D1 LoadLevel rejects error 3 before setup and preserves valid-load ordering");
	return failed;
}
