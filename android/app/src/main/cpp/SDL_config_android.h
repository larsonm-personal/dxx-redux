/*
 * SDL_config_android.h -- SDL 1.2 configuration for Android (NDK / bionic).
 *
 * Uses dummy video/audio/cdrom/joystick backends and pthread/unix for
 * threading and timing.  This is enough to compile and link the game;
 * backend-specific code is a no-op at runtime.
 */

#ifndef _SDL_config_h
#define _SDL_config_h

#include "SDL_platform.h"

/* -- standard C library --------------------------------------- */
#define STDC_HEADERS            1
#define HAVE_ALLOCA_H           1
#define HAVE_SYS_TYPES_H        1
#define HAVE_STDIO_H            1
#define HAVE_STDLIB_H           1
#define HAVE_STDARG_H           1
#define HAVE_MALLOC_H           1
#define HAVE_MEMORY_H           1
#define HAVE_STRING_H           1
#define HAVE_STRINGS_H          1
#define HAVE_INTTYPES_H         1
#define HAVE_STDINT_H           1
#define HAVE_CTYPE_H            1
#define HAVE_MATH_H             1
#define HAVE_SIGNAL_H           1

/* -- C standard functions ------------------------------------- */
#define HAVE_MALLOC             1
#define HAVE_CALLOC             1
#define HAVE_REALLOC            1
#define HAVE_FREE               1
#define HAVE_ALLOCA             1
#define HAVE_GETENV             1
#define HAVE_PUTENV             1
#define HAVE_UNSETENV           1
#define HAVE_QSORT              1
#define HAVE_ABS                1
#define HAVE_BCOPY              1
#define HAVE_MEMSET             1
#define HAVE_MEMCPY             1
#define HAVE_MEMMOVE            1
#define HAVE_MEMCMP             1
#define HAVE_STRLEN             1
#define HAVE_STRLCPY            1
#define HAVE_STRLCAT            1
#define HAVE_STRDUP             1
#define HAVE_STRCHR             1
#define HAVE_STRRCHR            1
#define HAVE_STRSTR             1
#define HAVE_STRTOL             1
#define HAVE_STRTOUL            1
#define HAVE_STRTOLL            1
#define HAVE_STRTOULL           1
#define HAVE_STRTOD             1
#define HAVE_ATOI               1
#define HAVE_ATOF               1
#define HAVE_STRCMP              1
#define HAVE_STRNCMP            1
#define HAVE_STRCASECMP         1
#define HAVE_STRNCASECMP        1
#define HAVE_SSCANF             1
#define HAVE_SNPRINTF           1
#define HAVE_VSNPRINTF          1
#define HAVE_SIGACTION          1
#define HAVE_SETJMP             1
#define HAVE_NANOSLEEP          1
#define HAVE_CLOCK_GETTIME      1
#define HAVE_DLOPEN             1
#define HAVE_MPROTECT           1
#define HAVE_SEM_TIMEDWAIT      1

/* -- backend selection ---------------------------------------- */
#define SDL_AUDIO_DRIVER_ANDROID        1
#define SDL_AUDIO_DRIVER_DUMMY          1
#define SDL_CDROM_DISABLED              1
#define SDL_JOYSTICK_DISABLED           1
#define SDL_LOADSO_DLOPEN               1
#define SDL_THREAD_PTHREAD              1
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1
#define SDL_TIMER_UNIX                  1
#define SDL_VIDEO_DRIVER_DUMMY          1

#endif /* _SDL_config_h */
