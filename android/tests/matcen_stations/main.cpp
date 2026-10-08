#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
extern "C" {
#include "fuelcen.h"
#include "matcen_mode.h"
#include "multi.h"
#include "multi_gameplay_options.h"
#include "state_android_shared.h"
#include "game.h"
}
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#include "../matcen_stations_fixture.hpp"
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "initialize isolated save fixture directory");
	test_matcen_stations(argv[1]);
	PHYSFS_deinit();
	return 0;
}
