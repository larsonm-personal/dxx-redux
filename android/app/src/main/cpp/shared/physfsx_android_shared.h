/* Shared Android PhysFS initialization for D1 and D2. */

#ifndef PHYSFSX_ANDROID_SHARED_H
#define PHYSFSX_ANDROID_SHARED_H

void physfsx_android_init(int argc, char *argv[], const char *game_dir);
void physfsx_android_mount_mission_directory(const char *descriptor);
void physfsx_android_unmount_mission_directory(void);

#endif /* PHYSFSX_ANDROID_SHARED_H */
