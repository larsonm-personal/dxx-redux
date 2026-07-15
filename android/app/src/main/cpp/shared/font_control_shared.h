#ifndef FONT_CONTROL_SHARED_H
#define FONT_CONTROL_SHARED_H

static inline int font_draw_control_sequence_length(const char *text)
{
	const unsigned char control = text ? (unsigned char) *text : 0;

	if (control == 1 || control == 2)
		return text[1] ? 2 : 1;
	if (control >= 3 && control <= 6)
		return 1;
	return 0;
}

#endif
