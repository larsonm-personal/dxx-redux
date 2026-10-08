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
		fprintf(stderr, "Usage: test_sow_direct <file.sow> <output_dir> [--append]\n"
		                "       test_sow_direct --volumes <output_dir> <file.sow>...\n"
		                "       test_sow_direct --directory <staged_disc>\n");
		return 1;
	}
	if (!strcmp(argv[1], "--volumes") || !strcmp(argv[1], "--directory")) {
		dxx_extract_attempt_budget_t budget;
		sow_file_list_t archives;
		int n;
		dxx_extract_attempt_budget_init(&budget, NULL, NULL);
		if (!strcmp(argv[1], "--directory")) {
			n = sow_extract_directory(argv[2], NULL, progress, NULL, &budget);
		} else {
			archives.count = argc - 3;
			if (archives.count <= 0 || archives.count > SOW_MAX_FILES) return 1;
			for (int i = 0; i < archives.count; ++i) {
				if (strlen(argv[i + 3]) >= SOW_PATH_LEN) return 1;
				strcpy(archives.paths[i], argv[i + 3]);
			}
			n = sow_extract_archives(&archives, argv[2], NULL, progress, NULL, &budget);
		}
		fprintf(stderr, "Extracted %d files\n", n);
		return n < 0 ? 1 : 0;
	}
	int append_existing = argc >= 4 && strcmp(argv[3], "--append") == 0;
	int n = sow_extract_with_mode(argv[1], argv[2], NULL, progress, NULL,
	                              append_existing);
	fprintf(stderr, "Extracted %d files\n", n);
	return (n >= 0) ? 0 : 1;
}
