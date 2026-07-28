#include "jni_string.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utf8_codec.h"

static void throw_text_exception(JNIEnv *env, const char *class_name, const char *message)
{
	jclass cls;
	if ((*env)->ExceptionCheck(env)) return;
	cls = (*env)->FindClass(env, class_name);
	if (cls) {
		(*env)->ThrowNew(env, cls, message);
		(*env)->DeleteLocalRef(env, cls);
	}
}

int dxx_jni_string_to_utf8(JNIEnv *env, jstring input, char **output)
{
	const jchar *chars;
	jsize units;
	size_t capacity, written;
	char *result;
	if (!env || !input || !output) return 0;
	*output = NULL;
	units = (*env)->GetStringLength(env, input);
	chars = (*env)->GetStringChars(env, input, NULL);
	if (!chars) return 0;
	if ((size_t) units > (SIZE_MAX - 1u) / 3u) {
		(*env)->ReleaseStringChars(env, input, chars);
		throw_text_exception(env, "java/lang/OutOfMemoryError", "JNI UTF-8 string is too large");
		return 0;
	}
	capacity = (size_t) units * 3u + 1u;
	result = (char *) malloc(capacity);
	if (!result) {
		(*env)->ReleaseStringChars(env, input, chars);
		throw_text_exception(env, "java/lang/OutOfMemoryError", "JNI UTF-8 allocation failed");
		return 0;
	}
	if (!dxx_utf16_to_utf8((const uint16_t *) chars, (size_t) units,
	                       result, capacity - 1u, &written, 1)) {
		free(result);
		(*env)->ReleaseStringChars(env, input, chars);
		throw_text_exception(env, "java/lang/IllegalArgumentException",
		                     "Java string is not valid null-free Unicode");
		return 0;
	}
	result[written] = '\0';
	(*env)->ReleaseStringChars(env, input, chars);
	*output = result;
	return 1;
}

jstring dxx_jni_string_from_utf8_n(JNIEnv *env, const char *input, size_t input_bytes)
{
	uint16_t *units;
	size_t written;
	jstring result;
	if (!env || (!input && input_bytes)) return NULL;
	if (input_bytes > INT_MAX ||
	    input_bytes > (SIZE_MAX / sizeof(*units)) - 1u) {
		throw_text_exception(env, "java/lang/OutOfMemoryError", "JNI UTF-16 string is too large");
		return NULL;
	}
	units = (uint16_t *) malloc((input_bytes + 1u) * sizeof(*units));
	if (!units) {
		throw_text_exception(env, "java/lang/OutOfMemoryError", "JNI UTF-16 allocation failed");
		return NULL;
	}
	if (!dxx_utf8_to_utf16(input, input_bytes, units, input_bytes + 1u, &written)) {
		free(units);
		throw_text_exception(env, "java/lang/IllegalArgumentException",
		                     "Native string is not valid UTF-8");
		return NULL;
	}
	result = (*env)->NewString(env, (const jchar *) units, (jsize) written);
	free(units);
	return result;
}

jstring dxx_jni_string_from_utf8(JNIEnv *env, const char *input)
{
	if (!input) return NULL;
	return dxx_jni_string_from_utf8_n(env, input, strlen(input));
}
