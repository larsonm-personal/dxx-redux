/*
 * Shared JNI overlay helpers -- send overlay text to the Kotlin UI layer.
 * Used by both D1 and D2 track_names.c.
 */

#ifndef _ANDROID_JNI_OVERLAY_H
#define _ANDROID_JNI_OVERLAY_H

/* Send a track name string to the Java showTrackName() overlay. No-op on non-Android. */
void android_send_track_name(const char *name);

/* Send a level name string to the Java showLevelName() overlay. No-op on non-Android. */
void android_send_level_name(const char *name);

/* Send a generic overlay line through the Android popup overlay. No-op on non-Android. */
void android_send_overlay_line(const char *text);

/* Publish graphics confirmation state independently of the game renderer */
void android_send_graphics_safety_state(const char *text);
/* Automation uses the launcher's real cross-process staging path */
void android_stage_launcher_graphics_for_automation(const char *text);

#endif
