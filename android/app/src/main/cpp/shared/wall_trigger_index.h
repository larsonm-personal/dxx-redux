#ifndef DXX_WALL_TRIGGER_INDEX_H
#define DXX_WALL_TRIGGER_INDEX_H

/* Packed wall records keep their byte layout; 255 is the missing sentinel */
static inline int wall_trigger_index(unsigned char encoded)
{
	return encoded == 255 ? -1 : encoded;
}

#endif
