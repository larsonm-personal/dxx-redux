#include "sha1.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_digest(dxx_sha1_ctx_t *ctx, const char *expected)
{
	uint8_t digest[20];
	char hex[42];
	memset(hex, '?', sizeof(hex));
	dxx_sha1_final(ctx, digest);
	dxx_sha1_hex(digest, hex);
	assert(strcmp(hex, expected) == 0);
	assert(hex[40] == '\0' && hex[41] == '?');
}

int main(void)
{
	/* Independent hashlib fixtures span both padding and block boundaries */
	static const struct {
		size_t len;
		const char *digest;
	} cases[] = {
		{ 0, "da39a3ee5e6b4b0d3255bfef95601890afd80709" },
		{ 1, "067d5096f219c64b53bb1c7d5e3754285b565a47" },
		{ 55, "c4622048cfef59b72875839ee7ae1cbcf55e7658" },
		{ 56, "ddc12942656468475970fa4fa49161f52ed138e4" },
		{ 63, "7f8c3fa49f1297bd8b9feb964b6b419987f9f0d1" },
		{ 64, "a334b47180c61fd522f99905ec02c36f9e848211" },
		{ 65, "dd27d9eb923d39687e10872c3e8133ba2f0a68a1" },
		{ 119, "bea949473b1ec34747ce121c3293624b5d9d8f84" },
		{ 120, "bf05266acd3ec21592b4d42aaea97fa6f3e51926" },
		{ 127, "b2b4bfd7b2112a167b77a600cca227593523c406" },
		{ 128, "3b1953091899492377f686c266b81d84b5d40f70" },
		{ 1024, "3d43695d5e945cea897d489cff9ff45bb019498b" },
	};
	static const size_t chunks[] = { 1, 7, 63, 64, 65, 1024 };
	uint8_t data[1024];
	dxx_sha1_ctx_t ctx;
	for (size_t i = 0; i < sizeof(data); i++)
		data[i] = (uint8_t) (i * 37 + 11);
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (size_t j = 0; j < sizeof(chunks) / sizeof(chunks[0]); j++) {
			dxx_sha1_init(&ctx);
			dxx_sha1_update(&ctx, NULL, 0);
			for (size_t offset = 0; offset < cases[i].len;) {
				size_t count = cases[i].len - offset;
				if (count > chunks[j]) count = chunks[j];
				dxx_sha1_update(&ctx, data + offset, count);
				offset += count;
			}
			check_digest(&ctx, cases[i].digest);
		}
	}

	dxx_sha1_init(&ctx);
	dxx_sha1_update(&ctx, (const uint8_t *) "abc", 3);
	check_digest(&ctx, "a9993e364706816aba3e25717850c26c9cd0d89d");
	memset(data, 'a', 1000);
	dxx_sha1_init(&ctx);
	for (int i = 0; i < 1000; i++)
		dxx_sha1_update(&ctx, data, 1000);
	check_digest(&ctx, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
	puts("SHA-1 vectors and chunk boundaries passed");
	return 0;
}
