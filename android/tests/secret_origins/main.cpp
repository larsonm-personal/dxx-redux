#include <cstdio>
#include <cstdlib>
extern "C" {
#include "dxxerror.h"
#include "u_mem.h"
#include "physfs.h"
}
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#include "../secret_origins_fixture.hpp"
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	// Android accepts null when this native process has no Java Activity
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "initialize isolated mission fixture directory");
	test_secret_origins(argv[1]);
	PHYSFS_deinit();
	return 0;
}
