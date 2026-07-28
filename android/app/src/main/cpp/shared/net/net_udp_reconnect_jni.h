#ifndef DXX_ANDROID_NET_UDP_RECONNECT_JNI_H
#define DXX_ANDROID_NET_UDP_RECONNECT_JNI_H

#include <stddef.h>
#include <stdint.h>

int android_net_udp_reconnect_get_public_key(uint8_t *output,
                                             size_t output_size);
int android_net_udp_reconnect_sign(const uint8_t *message,
                                   size_t message_size,
                                   uint8_t *signature,
                                   size_t signature_size);
int android_net_udp_reconnect_verify(const uint8_t *public_key,
                                     size_t public_key_size,
                                     const uint8_t *message,
                                     size_t message_size,
                                     const uint8_t *signature,
                                     size_t signature_size);
int android_net_udp_reconnect_random(uint8_t *output, size_t output_size);

#endif /* DXX_ANDROID_NET_UDP_RECONNECT_JNI_H */
