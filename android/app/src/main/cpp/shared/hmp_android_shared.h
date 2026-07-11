/* Shared Android HMP memory conversion for D1 and D2 */

#ifndef HMP_ANDROID_SHARED_H
#define HMP_ANDROID_SHARED_H

typedef unsigned int (*hmp_android_track_converter)(
    unsigned char *data, int size,
    unsigned char **midbuf, unsigned int *midlen);

int hmp_android_convert_mem(
    const unsigned char *hmp_data, int hmp_len,
    unsigned char **out_midi, int *out_len,
    hmp_android_track_converter convert_track,
    const unsigned char *tempo_track, unsigned int tempo_track_len);

/* On success, the caller owns *out_midi and must release it with d_free(). */
int hmp2mid_mem(const unsigned char *hmp_data, int hmp_len,
                unsigned char **out_midi, int *out_len);

#endif /* HMP_ANDROID_SHARED_H */
