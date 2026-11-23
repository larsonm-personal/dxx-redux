#pragma once

/* XL $CF and $CB define the two colors in custom slot zero */
static inline int xl_briefing_read_color(char **message, unsigned char foreground[3], unsigned char background[3])
{
	char *p = *message;
	if (*p != 'F' && *p != 'B') return 0;
	unsigned char *color = *p++ == 'F' ? foreground : background;
	unsigned char rgb[3];
	int count = 0;
	while (count < 3) {
		while (*p == ' ' || *p == '\t') ++p;
		if (*p < '0' || *p > '9') break;
		unsigned value = 0;
		while (*p >= '0' && *p <= '9' && value <= 255) value = value * 10 + (*p++ - '0');
		if (value > 255) break;
		rgb[count++] = (unsigned char) value;
	}
	while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
	if (count == 3 && (*p == '\n' || !*p))
		for (int i = 0; i < 3; ++i) color[i] = rgb[i];
	while (*p && *p != '\n') ++p;
	*message = *p ? p + 1 : p;
	return 1;
}
