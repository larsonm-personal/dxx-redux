#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
extern "C" {
#include "args.h"
#include "config.h"
#include "dxxerror.h"
#include "game.h"
#include "u_mem.h"
#include "physfs.h"
#include "player.h"
#include "playsave.h"
#include "songs.h"
}
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#include "../config_policy_fixture.hpp"
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	// Android accepts null when this native process has no Java Activity
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "initialize isolated config fixture directory");
	test_config_policy(argv[1]);
	PHYSFS_deinit();
	return 0;
}
