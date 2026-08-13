#ifndef RBAUDIO_BIN_H
#define RBAUDIO_BIN_H

/* Track control (multi-source support) */
int RBANextTrack(void);
int RBAPrevTrack(void);
int RBAPlaySpecificTrack(int track);
int RBAGetCurrentTrackInfo(int *out_track, char *out_name, int name_size,
                           int *out_source_index);
int RBAGetNumAudioTracks(void);
const char *RBAGetTrackName(int track);
int RBAIsAudioTrack(int track);
const char *RBAGetInitStatus(void);

#endif
