/* Shared Android HMP memory conversion for D1 and D2 */

#ifndef HMP_ANDROID_SHARED_H
#define HMP_ANDROID_SHARED_H

/* On success, the caller owns *out_midi and must release it with d_free(). */
int hmp2mid_mem(const unsigned char *hmp_data, int hmp_len,
                unsigned char **out_midi, int *out_len);

/* Playback export contains one pass, or a first pass plus a reusable repeat.
 * The repeat starts with HMI branch-table restoration, never song initialization.
 * Times include any silence after the last MIDI event (TML drops SMF EOT).
 */
struct hmp_playback_info {
	double repeat_ms;
	double end_ms;
	int branch_loop;
	int unsupported_branches;
	int filtered_tracks;
	int no_gm_arrangement;
};

int hmp2mid_playback_mem(const unsigned char *data, int len, int repeat,
                         unsigned char **midi, int *midi_len,
                         struct hmp_playback_info *info);

#endif /* HMP_ANDROID_SHARED_H */
