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
        fprintf(stderr, "Usage: test_sow_direct <file.sow> <output_dir>\n");
        return 1;
    }
    int n = sow_extract(argv[1], argv[2], NULL, progress, NULL);
    fprintf(stderr, "Extracted %d files\n", n);
    return (n >= 0) ? 0 : 1;
}
