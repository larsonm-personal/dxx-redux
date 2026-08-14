#include "net_udp_reconnect_jni.h"

#include <jni.h>

extern JavaVM *g_jvm;
extern jobject g_activity;

typedef struct android_jni_scope {
	JNIEnv *env;
	int attached;
} android_jni_scope;

static int android_jni_scope_enter(android_jni_scope *scope)
{
	jint result;

	scope->env = NULL;
	scope->attached = 0;
	if (!g_jvm || !g_activity)
		return 0;

	result = (*g_jvm)->GetEnv(g_jvm, (void **) &scope->env, JNI_VERSION_1_6);
	if (result == JNI_OK)
		return 1;
	if (result != JNI_EDETACHED ||
	    (*g_jvm)->AttachCurrentThread(g_jvm, &scope->env, NULL) != JNI_OK)
		return 0;
	scope->attached = 1;
	return 1;
}

static void android_jni_scope_leave(android_jni_scope *scope)
{
	if (scope->attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}

static int android_jni_check(android_jni_scope *scope)
{
	return !(*scope->env)->ExceptionCheck(scope->env);
}

static jbyteArray android_jni_new_byte_array(JNIEnv *env,
                                             const uint8_t *data,
                                             size_t size)
{
	jbyteArray result;

	if (size > 0x7fffffff)
		return NULL;
	result = (*env)->NewByteArray(env, (jsize) size);
	if (!result || (*env)->ExceptionCheck(env)) return NULL;
	if (size) {
		(*env)->SetByteArrayRegion(env, result, 0, (jsize) size,
		                           (const jbyte *) data);
		if ((*env)->ExceptionCheck(env)) return NULL;
	}
	return result;
}

static int android_jni_copy_byte_array(JNIEnv *env, jbyteArray source,
                                       uint8_t *output, size_t output_size)
{
	jsize size;

	if (!source) return 0;
	size = (*env)->GetArrayLength(env, source);
	if ((*env)->ExceptionCheck(env) || size <= 0 || (size_t) size > output_size)
		return 0;
	(*env)->GetByteArrayRegion(env, source, 0, size, (jbyte *) output);
	if ((*env)->ExceptionCheck(env)) return 0;
	return size;
}

static jclass android_jni_identity_class(android_jni_scope *scope)
{
	jclass activity_class = NULL;
	jclass class_loader_class = NULL;
	jclass identity_class = NULL;
	jmethodID get_class_loader = NULL;
	jmethodID load_class = NULL;
	jobject class_loader = NULL;
	jstring class_name = NULL;

	activity_class =
	    (*scope->env)->GetObjectClass(scope->env, g_activity);
	if (!activity_class || !android_jni_check(scope))
		goto cleanup;
	get_class_loader = (*scope->env)->GetMethodID(scope->env, activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
	if (!get_class_loader || !android_jni_check(scope))
		goto cleanup;
	class_loader = (*scope->env)->CallObjectMethod(scope->env, g_activity, get_class_loader);
	if (!class_loader || !android_jni_check(scope))
		goto cleanup;
	class_loader_class =
	    (*scope->env)->GetObjectClass(scope->env, class_loader);
	if (!class_loader_class || !android_jni_check(scope))
		goto cleanup;
	load_class = (*scope->env)->GetMethodID(scope->env, class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	if (!load_class || !android_jni_check(scope))
		goto cleanup;
	class_name = (*scope->env)->NewStringUTF(scope->env, "com.dxxredux.app.multiplayer.UdpReconnectIdentity");
	if (!class_name || !android_jni_check(scope))
		goto cleanup;
	identity_class = (jclass) (*scope->env)->CallObjectMethod(scope->env, class_loader, load_class, class_name);
	if (!android_jni_check(scope))
		identity_class = NULL;

cleanup:
	if (!(*scope->env)->ExceptionCheck(scope->env) && class_name)
		(*scope->env)->DeleteLocalRef(scope->env, class_name);
	if (!(*scope->env)->ExceptionCheck(scope->env) && class_loader_class)
		(*scope->env)->DeleteLocalRef(scope->env, class_loader_class);
	if (!(*scope->env)->ExceptionCheck(scope->env) && class_loader)
		(*scope->env)->DeleteLocalRef(scope->env, class_loader);
	if (!(*scope->env)->ExceptionCheck(scope->env) && activity_class)
		(*scope->env)->DeleteLocalRef(scope->env, activity_class);
	return identity_class;
}

int android_net_udp_reconnect_get_public_key(uint8_t *output,
                                             size_t output_size)
{
	android_jni_scope scope;
	jclass identity_class;
	jmethodID method;
	jbyteArray key;
	int result = 0;

	if (!output || !android_jni_scope_enter(&scope))
		return 0;
	identity_class = android_jni_identity_class(&scope);
	method = identity_class
	             ? (*scope.env)->GetStaticMethodID(scope.env, identity_class, "publicKey", "()[B")
	             : NULL;
	key = method ? (jbyteArray) (*scope.env)->CallStaticObjectMethod(scope.env, identity_class, method)
	             : NULL;
	if (android_jni_check(&scope)) {
		result = android_jni_copy_byte_array(scope.env, key, output,
		                                     output_size);
		if (!android_jni_check(&scope))
			result = 0;
	}
	if (key && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, key);
	if (identity_class && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, identity_class);
	android_jni_scope_leave(&scope);
	return result;
}

int android_net_udp_reconnect_sign(const uint8_t *message,
                                   size_t message_size,
                                   uint8_t *signature,
                                   size_t signature_size)
{
	android_jni_scope scope;
	jclass identity_class;
	jmethodID method;
	jbyteArray java_message;
	jbyteArray java_signature;
	int result = 0;

	if (!message || !signature || !android_jni_scope_enter(&scope))
		return 0;
	identity_class = android_jni_identity_class(&scope);
	method = identity_class
	             ? (*scope.env)->GetStaticMethodID(scope.env, identity_class, "sign", "([B)[B")
	             : NULL;
	java_message = method
	                   ? android_jni_new_byte_array(scope.env, message,
	                                                message_size)
	                   : NULL;
	java_signature =
	    java_message
	        ? (jbyteArray) (*scope.env)->CallStaticObjectMethod(scope.env, identity_class, method, java_message)
	        : NULL;
	if (android_jni_check(&scope)) {
		result = android_jni_copy_byte_array(scope.env, java_signature,
		                                     signature, signature_size);
		if (!android_jni_check(&scope))
			result = 0;
	}
	if (java_signature && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, java_signature);
	if (java_message && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, java_message);
	if (identity_class && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, identity_class);
	android_jni_scope_leave(&scope);
	return result;
}

int android_net_udp_reconnect_verify(const uint8_t *public_key,
                                     size_t public_key_size,
                                     const uint8_t *message,
                                     size_t message_size,
                                     const uint8_t *signature,
                                     size_t signature_size)
{
	android_jni_scope scope;
	jclass identity_class;
	jmethodID method;
	jbyteArray java_key;
	jbyteArray java_message;
	jbyteArray java_signature;
	jboolean verified = JNI_FALSE;

	if (!public_key || !message || !signature ||
	    !android_jni_scope_enter(&scope))
		return 0;
	identity_class = android_jni_identity_class(&scope);
	method = identity_class
	             ? (*scope.env)->GetStaticMethodID(scope.env, identity_class, "verify", "([B[B[B)Z")
	             : NULL;
	java_key = method ? android_jni_new_byte_array(
	                        scope.env, public_key, public_key_size)
	                  : NULL;
	java_message = java_key
	                   ? android_jni_new_byte_array(scope.env, message,
	                                                message_size)
	                   : NULL;
	java_signature =
	    java_message
	        ? android_jni_new_byte_array(scope.env, signature, signature_size)
	        : NULL;
	if (java_signature && android_jni_check(&scope))
		verified = (*scope.env)->CallStaticBooleanMethod(scope.env, identity_class, method, java_key, java_message, java_signature);
	if (!android_jni_check(&scope))
		verified = JNI_FALSE;
	if (java_signature)
		(*scope.env)->DeleteLocalRef(scope.env, java_signature);
	if (java_message && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, java_message);
	if (java_key && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, java_key);
	if (identity_class && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, identity_class);
	android_jni_scope_leave(&scope);
	return verified == JNI_TRUE;
}

int android_net_udp_reconnect_random(uint8_t *output, size_t output_size)
{
	android_jni_scope scope;
	jclass identity_class;
	jmethodID method;
	jbyteArray bytes;
	int result = 0;

	if (!output || output_size > 0x7fffffff ||
	    !android_jni_scope_enter(&scope))
		return 0;
	identity_class = android_jni_identity_class(&scope);
	method = identity_class
	             ? (*scope.env)->GetStaticMethodID(scope.env, identity_class, "randomBytes", "(I)[B")
	             : NULL;
	bytes = method ? (jbyteArray) (*scope.env)->CallStaticObjectMethod(scope.env, identity_class, method, (jint) output_size)
	               : NULL;
	if (android_jni_check(&scope)) {
		result = android_jni_copy_byte_array(scope.env, bytes, output,
		                                     output_size);
		if (!android_jni_check(&scope))
			result = 0;
	}
	if (bytes && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, bytes);
	if (identity_class && android_jni_check(&scope))
		(*scope.env)->DeleteLocalRef(scope.env, identity_class);
	android_jni_scope_leave(&scope);
	return result == (int) output_size;
}
