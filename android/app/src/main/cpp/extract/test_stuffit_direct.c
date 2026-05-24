#include <stdio.h>

#include "game_file_extensions.h"
#include "stuffit_extract.h"

static int progress(const char *fname, long long done, long long total, void *ud)
{
	(void) ud;
	fprintf(stderr, "  %s  %lld / %lld\n", fname, done, total);
	return 0;
}

int main(int argc, char *argv[])
{
	int n;

	if (argc < 3) {
		fprintf(stderr, "Usage: test_stuffit_direct <file.sit> <output_dir>\n");
		return 1;
	}
	n = stuffit_extract(argv[1], argv[2], dxx_android_mac_disc_extract_extensions,
	                    progress, NULL);
	fprintf(stderr, "Extracted %d files\n", n);
	return (n >= 0) ? 0 : 1;
}