#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input_demo_rng_mode.h"

static int is_identifier_char(int c)
{
	return isalnum(c) || c == '_';
}

static const char *find_rng_mode_key(const char *text)
{
	const char *match = text;

	while (match && *match) {
		match = strstr(match, "rng_mode");
		if (!match)
			return NULL;
		if ((match == text || !is_identifier_char((unsigned char) match[-1])) &&
		    !is_identifier_char((unsigned char) match[8]))
			return match;
		match += 8;
	}

	return NULL;
}

static const char *read_text_file(const char *path, char **text_out)
{
	FILE *f;
	char *text;
	long size;
	size_t read_size;

	*text_out = NULL;
	if (!path)
		return "missing demo file path";
	f = fopen(path, "rb");
	if (!f)
		return "could not open demo file";
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return "could not size demo file";
	}
	size = ftell(f);
	if (size < 0) {
		fclose(f);
		return "could not size demo file";
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return "could not rewind demo file";
	}
	text = (char *) malloc((size_t) size + 1);
	if (!text) {
		fclose(f);
		return "could not allocate demo file buffer";
	}
	read_size = fread(text, 1, (size_t) size, f);
	fclose(f);
	if (read_size != (size_t) size) {
		free(text);
		return "could not read demo file";
	}
	text[size] = '\0';
	*text_out = text;
	return NULL;
}

int input_demo_rng_mode_parse(const char *text)
{
	if (!text)
		return 0;
	if (!strcmp(text, "lcg_state"))
		return D_RAND_REPLAY_MODE_LCG_STATE;
	if (!strcmp(text, "libc_reseed"))
		return D_RAND_REPLAY_MODE_LIBC_RESEED;
	if (!strcmp(text, "output_log"))
		return D_RAND_REPLAY_MODE_OUTPUT_LOG;
	return 0;
}

const char *input_demo_rng_mode_name(int mode)
{
	switch (mode) {
		case D_RAND_REPLAY_MODE_LCG_STATE:
			return "lcg_state";
		case D_RAND_REPLAY_MODE_LIBC_RESEED:
			return "libc_reseed";
		case D_RAND_REPLAY_MODE_OUTPUT_LOG:
			return "output_log";
		default:
			return "invalid";
	}
}

int input_demo_rng_mode_is_compatible(int fixture_mode, int engine_mode)
{
	if (fixture_mode == D_RAND_REPLAY_MODE_OUTPUT_LOG)
		return 0;
	if (fixture_mode != D_RAND_REPLAY_MODE_LCG_STATE &&
	    fixture_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return 0;
	return fixture_mode == engine_mode;
}

const char *input_demo_rng_mode_parse_metadata_text(const char *text, int *mode)
{
	char value[32];
	const char *key;
	const char *cursor;
	char quote;
	size_t value_len = 0;
	int parsed_mode;

	if (!text)
		return "missing demo file text";
	if (!mode)
		return "missing rng_mode output";
	key = find_rng_mode_key(text);
	if (!key)
		return "demo file is missing rng_mode";
	cursor = key + 8;
	if (*cursor == '"' || *cursor == '\'')
		cursor++;
	while (*cursor && isspace((unsigned char) *cursor))
		cursor++;
	if (*cursor != ':')
		return "demo file rng_mode is missing ':'";
	cursor++;
	while (*cursor && isspace((unsigned char) *cursor))
		cursor++;
	if (*cursor != '"' && *cursor != '\'')
		return "demo file rng_mode must be a quoted string";
	quote = *cursor++;
	while (*cursor && *cursor != quote) {
		if (*cursor == '\\' && cursor[1])
			cursor++;
		if (value_len + 1 >= sizeof(value))
			return "demo file rng_mode is too long";
		value[value_len++] = *cursor++;
	}
	if (*cursor != quote)
		return "demo file rng_mode string is unterminated";
	value[value_len] = '\0';
	parsed_mode = input_demo_rng_mode_parse(value);
	if (!parsed_mode)
		return "demo file rng_mode is invalid";
	*mode = parsed_mode;
	return NULL;
}

const char *input_demo_rng_mode_validate_metadata_text(const char *text, int engine_mode,
                                                       int *mode)
{
	const char *error = input_demo_rng_mode_parse_metadata_text(text, mode);
	if (error)
		return error;
	if (!input_demo_rng_mode_is_compatible(*mode, engine_mode))
		return "demo file rng_mode is incompatible with the active RNG backend";
	return NULL;
}

const char *input_demo_rng_mode_validate_metadata_file(const char *path, int engine_mode,
                                                       int *mode)
{
	char *text;
	const char *error = read_text_file(path, &text);
	if (error)
		return error;
	error = input_demo_rng_mode_validate_metadata_text(text, engine_mode, mode);
	free(text);
	return error;
}