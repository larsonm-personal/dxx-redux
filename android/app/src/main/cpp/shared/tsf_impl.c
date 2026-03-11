/* TinySoundFont + TinyMidiLoader — shared library implementation.
 * Built as libtsf.so, linked by both game .so files via arch_sdl.
 * Consumer (digi_tsf_music.c) includes headers without IMPLEMENTATION defines. */

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"

#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"
