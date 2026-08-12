#include "pilot_pref_transaction.h"

#include <cstdio>
#include <cstring>

namespace
{
struct patch_context {
	const char *replacement;
	bool succeed;
};

int replace_file(const char *path, void *opaque)
{
	auto *context = static_cast<patch_context *>(opaque);
	FILE *file = std::fopen(path, "wb");
	if (!file) return 0;
	const size_t size = std::strlen(context->replacement);
	const bool wrote = std::fwrite(context->replacement, 1, size, file) == size;
	const bool closed = std::fclose(file) == 0;
	return wrote && closed && context->succeed;
}

bool contents_equal(const char *path, const char *expected)
{
	char buffer[64]{};
	FILE *file = std::fopen(path, "rb");
	if (!file) return false;
	const size_t size = std::fread(buffer, 1, sizeof(buffer), file);
	const bool closed = std::fclose(file) == 0;
	return closed && size == std::strlen(expected) && !std::memcmp(buffer, expected, size);
}

bool write_fixture(const char *path, const char *contents)
{
	FILE *file = std::fopen(path, "wb");
	if (!file) return false;
	const size_t size = std::strlen(contents);
	return std::fwrite(contents, 1, size, file) == size && std::fclose(file) == 0;
}
} // namespace

int main()
{
	const char *first = "pilot-pref-transaction-1.tmp";
	const char *second = "pilot-pref-transaction-2.tmp";
	if (!write_fixture(first, "first-old") || !write_fixture(second, "second-old")) return 1;

	patch_context first_patch{ "first-new", true };
	patch_context failed_patch{ "second-new", false };
	pilot_pref_patch_target failed_targets[] = {
		{ first, replace_file, &first_patch },
		{ second, replace_file, &failed_patch },
	};
	if (pilot_pref_patch_transaction(failed_targets, 2) != -1 ||
	    !contents_equal(first, "first-old") || !contents_equal(second, "second-old")) return 2;

	patch_context second_patch{ "second-new", true };
	pilot_pref_patch_target successful_targets[] = {
		{ first, replace_file, &first_patch },
		{ second, replace_file, &second_patch },
	};
	if (pilot_pref_patch_transaction(successful_targets, 2) != 2 ||
	    !contents_equal(first, "first-new") || !contents_equal(second, "second-new")) return 3;

	std::remove(first);
	std::remove(second);
	return 0;
}
