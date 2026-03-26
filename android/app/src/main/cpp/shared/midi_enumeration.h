/*
 * midi_enumeration.h -- Enumerate MIDI/HMP tracks from game HOG files
 *                       and mission directories.
 *
 * Scans game data for playable MIDI tracks and returns a JSON string
 * describing available sources and tracks (consumed by Kotlin).
 */

#ifndef MIDI_ENUMERATION_H
#define MIDI_ENUMERATION_H

/*
 * Enumerate all MIDI/HMP tracks in the game data directory.
 *
 * files_dir: absolute path to the app's files directory (contains
 *            game HOGs like descent2.hog, descent.hog, and missions/)
 *
 * Returns a malloc'd JSON string describing all sources and tracks.
 * Caller must free() the result.
 *
 * JSON format:
 * {
 *   "sources": [
 *     {
 *       "id": "d2-builtin",
 *       "label": "Descent 2 (built-in)",
 *       "game": "d2",
 *       "hog": "descent2.hog",
 *       "tracks": [
 *         { "filename": "game01.hmp", "duration_ms": 180000 },
 *         ...
 *       ]
 *     },
 *     ...
 *   ]
 * }
 */
char *midi_enumerate_tracks(const char *files_dir);

#endif /* MIDI_ENUMERATION_H */
