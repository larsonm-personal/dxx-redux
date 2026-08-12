#include <stdio.h>
#include <string.h>

#include "android_save_set.h"

static int failures = 0;

static void expect_string(const char *label, const char *expected,
                          const char *actual)
{
	if (strcmp(expected, actual)) {
		printf("%s: expected '%s', got '%s'\n", label, expected, actual);
		failures++;
	}
}

static void expect_true(const char *label, int value)
{
	if (!value) {
		printf("%s: expected true\n", label);
		failures++;
	}
}

int main(void)
{
	char path[256];
	char other[256];
	char component[32];

	android_save_set_sanitize_component(component, sizeof(component),
	                                    "../Pilot One", "player");
	expect_string("sanitized component", "pilot_one", component);

	expect_true("single d2 path",
	            android_save_set_build_slot_path(
	                path, sizeof(path), 1, "single", "Neuma", "d2", 0, 0));
	expect_string("single d2 path value",
	              "Players/save_sets/single/neuma/d2/neuma.sg0", path);

	expect_true("single first strike path",
	            android_save_set_build_slot_path(
	                other, sizeof(other), 1, "single", "Neuma", "descent",
	                0, 0));
	expect_string("single first strike path value",
	              "Players/save_sets/single/neuma/descent/neuma.sg0", other);
	if (!strcmp(path, other)) {
		printf("mission paths should differ\n");
		failures++;
	}

	expect_true("long mission A path",
	            android_save_set_build_slot_path(
	                path, sizeof(path), 1, "single", "Neuma",
	                "abcdefghijklmnopqrstuvwxyzaaaaaa", 0, 0));
	expect_true("long mission B path",
	            android_save_set_build_slot_path(
	                other, sizeof(other), 1, "single", "Neuma",
	                "abcdefghijklmnopqrstuvwxyzbbbbbb", 0, 0));
	if (!strcmp(path, other)) {
		printf("long mission paths sharing a prefix should differ\n");
		failures++;
	}

	expect_true("coop autosave path",
	            android_save_set_build_slot_path(
	                path, sizeof(path), 1, "coop",
	                ANDROID_SAVE_SET_COOP_CALLSIGN, "d2x", 7, 1));
	expect_string("coop autosave path value",
	              "Players/save_sets/coop/d2x/coopsave.mg7", path);

	expect_true("secret companion path",
	            android_save_set_build_secret_path(path, sizeof(path), 1,
	                                               "Neuma", "d2", 7));
	expect_string("secret companion path value",
	              "Players/save_sets/single/neuma/d2/7secret.sgc", path);

	expect_true("local sidecar path",
	            android_save_set_build_sidecar_path(
	                path, sizeof(path), 0, "coop", "coopsave", "d2",
	                "coop_autosave_history.json"));
	expect_string("local sidecar path value",
	              "save_sets/coop/d2/coop_autosave_history.json", path);

	return failures ? 1 : 0;
}
