#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mission_descriptor_safe.h"

static void expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

int main(void)
{
	char missing[] = "briefing";
	char empty[] = "briefing =   \r\n";
	char present[] = "briefing =\tbrief.tex\r\n";
	char output[13];
	int value;
	unsigned extra_origins;

	expect(mission_descriptor_key_is("  D2X-Name \t= Arena", "d2x-name"), "XL key ignores case and surrounding whitespace");
	expect(!mission_descriptor_key_is("d2x-name-extra = Arena", "d2x-name"), "XL key suffix rejected");
	expect(!mission_descriptor_key_is("d2x-name", "d2x-name"), "XL key requires equals");
	expect(!mission_descriptor_key_is("d2x", "d2x-name"), "truncated XL key rejected");
	expect(!mission_descriptor_key_is(NULL, "d2x-name"), "null XL key rejected");
	expect(!mission_descriptor_key_is("d2x-name = Arena", ""), "empty expected key rejected");

	{
		char title[26];
		expect(mission_descriptor_copy_title("  Observatory XL ; comment", title, sizeof(title)) &&
		           !strcmp(title, "Observatory XL"),
		       "title whitespace and comment handled");
		expect(mission_descriptor_copy_title("1234567890123456789012345", title, sizeof(title)), "maximum native title accepted");
		expect(!mission_descriptor_copy_title("12345678901234567890123456", title, sizeof(title)), "overlong title rejected");
		expect(!mission_descriptor_copy_title("A                                                                                B", title, sizeof(title)), "late nonspace title byte is not truncated away");
		expect(!mission_descriptor_copy_title(" ; comment", title, sizeof(title)), "comment-only title rejected");
		expect(!mission_descriptor_copy_title(NULL, title, sizeof(title)), "null title rejected");
	}

	expect(!mission_descriptor_value(NULL), "null line has no value");
	expect(!mission_descriptor_value(missing), "missing equals has no value");
	expect(!mission_descriptor_value(empty), "empty value is rejected");
	expect(!strncmp(mission_descriptor_value(present), "brief.tex", 9), "leading value whitespace is skipped");

	expect(mission_descriptor_parse_count("1\r\n", 1, 127, &value) && value == 1, "minimum count parses");
	expect(mission_descriptor_parse_count("127 ; comment", 1, 127, &value) && value == 127, "maximum count parses");
	expect(mission_descriptor_parse_count("0", 0, 127, &value) && value == 0, "zero secret count parses");
	expect(!mission_descriptor_parse_count("-1", 0, 127, &value), "negative count is rejected");
	expect(!mission_descriptor_parse_count("128", 0, 127, &value), "oversized count is rejected");
	expect(!mission_descriptor_parse_count("12junk", 0, 127, &value), "count suffix is rejected");
	expect(!mission_descriptor_parse_count("999999999999999999999", 0, 127, &value), "overflowing count is rejected");

	expect(mission_descriptor_make_text_filename("12345678", ".tex", output, sizeof(output)) &&
	           !strcmp(output, "12345678.tex"),
	       "largest fitting text stem is accepted");
	expect(!mission_descriptor_make_text_filename("123456789", ".tex", output, sizeof(output)),
	       "overlong text stem is rejected");
	expect(mission_descriptor_make_text_filename("briefing.txb", ".tex", output, sizeof(output)) &&
	           !strcmp(output, "briefing.tex"),
	       "existing extension is replaced");
	expect(!mission_descriptor_make_text_filename("../brief", ".tex", output, sizeof(output)),
	       "text path traversal is rejected");

	expect(!mission_descriptor_copy_level_name(NULL, output, sizeof(output)), "truncated level list is rejected");
	expect(mission_descriptor_copy_level_name("level01.rdl\r\n", output, sizeof(output)) &&
	           !strcmp(output, "level01.rdl"),
	       "bounded level name parses");
	expect(!mission_descriptor_copy_level_name("1234567890123.rdl", output, sizeof(output)),
	       "overlong level name is rejected");

	expect(mission_descriptor_parse_secret("secret.rdl, 3\r\n", 3, output, sizeof(output), &value) &&
	           !strcmp(output, "secret.rdl") && value == 3,
	       "bounded secret entry parses");
	expect(mission_descriptor_parse_secret("secret.rdl, 2, 3 ; comment", 3, output, sizeof(output), &value) &&
	           !strcmp(output, "secret.rdl") && value == 2,
	       "multiple valid secret origins preserve the first origin");
	expect(mission_descriptor_parse_secret_ex("secret.rdl, 2, 3 ; comment", 3, output, sizeof(output),
	                                          &value, &extra_origins) &&
	           value == 2 && extra_origins == 1,
	       "ignored secret origins are reported to the caller");
	expect(!mission_descriptor_parse_secret(NULL, 3, output, sizeof(output), &value),
	       "truncated secret list is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl", 3, output, sizeof(output), &value),
	       "secret without origin is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl, 4", 3, output, sizeof(output), &value),
	       "secret origin beyond the level list is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl, 2, 4", 3, output, sizeof(output), &value),
	       "out-of-range trailing secret origin is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl, 2, bad", 3, output, sizeof(output), &value),
	       "malformed trailing secret origin is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl, 2, ,3", 3, output, sizeof(output), &value),
	       "empty middle secret origin is rejected");
	expect(!mission_descriptor_parse_secret("secret.rdl, 2,", 3, output, sizeof(output), &value),
	       "empty trailing secret origin is rejected");

	puts("PASS: bounded mission descriptor tokens");
	return 0;
}
