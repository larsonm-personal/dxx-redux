/* Quick standalone test: extract a .sow file directly via sow_extract() */
#include <stdio.h>
#include <string.h>
#include "sow_extract.h"

static int progress(const char *fname, long long done, long long total, void *ud)
{
	fprintf(stderr, "  %s  %lld / %lld\n", fname, done, total);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: test_sow_direct <file.sow> <output_dir> [--append]\n");
		return 1;
	}
	int append_existing = argc >= 4 && strcmp(argv[3], "--append") == 0;
	int n = sow_extract_with_mode(argv[1], argv[2], NULL, progress, NULL,
	                              append_existing);
	fprintf(stderr, "Extracted %d files\n", n);
	return (n >= 0) ? 0 : 1;
}
