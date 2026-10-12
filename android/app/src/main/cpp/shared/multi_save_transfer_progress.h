#ifndef MULTI_SAVE_TRANSFER_PROGRESS_H
#define MULTI_SAVE_TRANSFER_PROGRESS_H

/* Display-only snapshot on the engine thread; percent is -1 outside downloads
 * The returned text lasts until the next call and never controls the transfer */
#ifdef __cplusplus
extern "C" {
#endif
const char *multi_save_transfer_status(int *percent);
#ifdef __cplusplus
}
#endif

#endif
