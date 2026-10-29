#include <jni.h>
#include "../../../../../XL_compat/hog_format.h"

struct java_stream {
	JNIEnv *env;
	jobject input;
	jmethodID read;
	jbyteArray buffer;
};

static void io_error(JNIEnv *env, const char *message)
{
	jclass exception;
	if ((*env)->ExceptionCheck(env)) return;
	exception = (*env)->FindClass(env, "java/io/IOException");
	if (exception) (*env)->ThrowNew(env, exception, message);
}

static size_t read_java(void *context, unsigned char *out, size_t count)
{
	struct java_stream *stream = (struct java_stream *) context;
	JNIEnv *env = stream->env;
	jint got;
	if ((*env)->ExceptionCheck(env)) return 0;
	got = (*env)->CallIntMethod(env, stream->input, stream->read, stream->buffer, (jint) 0, (jint) count);
	if ((*env)->ExceptionCheck(env) || got == -1) return 0;
	if (got <= 0 || (size_t) got > count) {
		io_error(env, "HOG input stream returned an invalid read count");
		return 0;
	}
	(*env)->GetByteArrayRegion(env, stream->buffer, 0, got, (jbyte *) out);
	return (*env)->ExceptionCheck(env) ? 0 : (size_t) got;
}

static int open_stream(JNIEnv *env, jobject input, struct java_stream *stream)
{
	jclass input_class;
	memset(stream, 0, sizeof(*stream));
	stream->env = env;
	stream->input = input;
	if (!input) {
		io_error(env, "Missing HOG input stream");
		return 0;
	}
	input_class = (*env)->GetObjectClass(env, input);
	if (!input_class) return 0;
	stream->read = (*env)->GetMethodID(env, input_class, "read", "([BII)I");
	(*env)->DeleteLocalRef(env, input_class);
	if (!stream->read) return 0;
	stream->buffer = (*env)->NewByteArray(env, DXX_HOG_LONG_NAME_BYTES);
	return stream->buffer != NULL;
}

JNIEXPORT void JNICALL Java_com_dxxredux_app_NativeHogArchive_nativeReadSignature(JNIEnv *env, jclass type, jobject input)
{
	struct java_stream stream;
	unsigned char signature[DXX_HOG_SIGNATURE_BYTES];
	(void) type;
	if (!open_stream(env, input, &stream)) return;
	if (dxx_hog_read_exact(read_java, &stream, signature, sizeof(signature)) != sizeof(signature))
		io_error(env, "Truncated HOG signature");
	else if (!dxx_hog_signature(signature))
		io_error(env, "Invalid HOG signature (expected DHF or D2X)");
	(*env)->DeleteLocalRef(env, stream.buffer);
}

JNIEXPORT jobject JNICALL Java_com_dxxredux_app_NativeHogArchive_nativeReadEntry(JNIEnv *env, jclass type, jobject input)
{
	struct java_stream stream;
	struct dxx_hog_header entry;
	const char *error = NULL;
	jclass entry_class;
	jmethodID constructor;
	jstring name;
	jchar name_chars[DXX_HOG_LONG_NAME_BYTES];
	jsize length;
	int status;
	jobject result = NULL;
	(void) type;
	if (!open_stream(env, input, &stream)) return NULL;
	status = dxx_hog_read_header(read_java, &stream, &entry, &error);
	(*env)->DeleteLocalRef(env, stream.buffer);
	if ((*env)->ExceptionCheck(env)) return NULL;
	if (status < 0) {
		io_error(env, error);
		return NULL;
	}
	if (!status) return NULL;
	entry_class = (*env)->FindClass(env, "com/dxxredux/app/NativeHogArchive$Entry");
	if (!entry_class) return NULL;
	constructor = (*env)->GetMethodID(env, entry_class, "<init>", "(Ljava/lang/String;JZI)V");
	if (!constructor) {
		(*env)->DeleteLocalRef(env, entry_class);
		return NULL;
	}
	// Preserve filename bytes; NewStringUTF would misinterpret legacy bytes as UTF-8
	for (length = 0; entry.name[length]; ++length) name_chars[length] = (unsigned char) entry.name[length];
	name = (*env)->NewString(env, name_chars, length);
	if (name) {
		result = (*env)->NewObject(env, entry_class, constructor, name, (jlong) entry.size, (jboolean) entry.extended, (jint) entry.header_bytes);
		(*env)->DeleteLocalRef(env, name);
	}
	(*env)->DeleteLocalRef(env, entry_class);
	return result;
}
