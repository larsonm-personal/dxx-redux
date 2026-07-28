#ifndef DXX_JNI_STRING_H
#define DXX_JNI_STRING_H

#include <jni.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int dxx_jni_string_to_utf8(JNIEnv *env, jstring input, char **output);
jstring dxx_jni_string_from_utf8(JNIEnv *env, const char *input);
jstring dxx_jni_string_from_utf8_n(JNIEnv *env, const char *input, size_t input_bytes);

#ifdef __cplusplus
}
#endif

#endif
