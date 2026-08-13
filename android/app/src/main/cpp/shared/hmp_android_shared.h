/* Shared Android HMP memory conversion for D1 and D2 */

#ifndef HMP_ANDROID_SHARED_H
#define HMP_ANDROID_SHARED_H

/* On success, the caller owns *out_midi and must release it with d_free(). */
int hmp2mid_mem(const unsigned char *hmp_data, int hmp_len,
                unsigned char **out_midi, int *out_len);

#endif /* HMP_ANDROID_SHARED_H */
