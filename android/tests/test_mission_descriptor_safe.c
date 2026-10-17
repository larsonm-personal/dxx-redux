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
