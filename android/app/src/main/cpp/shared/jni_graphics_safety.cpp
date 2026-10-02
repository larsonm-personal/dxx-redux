#include <jni.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "graphics_safety_store.h"
#include "graphics_config_transaction.h"
#include "jni_string.h"

namespace
{
using native_string = std::unique_ptr<char, decltype(&std::free)>;
native_string copy_string(JNIEnv *env, jstring value)
{
	char *text = nullptr;
	dxx_jni_string_to_utf8(env, value, &text);
	return native_string(text, &std::free);
}
} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_dxxredux_app_NativeGraphicsSafety_nativeLock(JNIEnv *env, jobject, jstring jroot)
{
	auto root = copy_string(env, jroot);
	return root ? static_cast<jlong>(reinterpret_cast<uintptr_t>(graphics_safety_lock(root.get()))) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_dxxredux_app_NativeGraphicsSafety_nativeUnlock(JNIEnv *, jobject, jlong lock)
{
	graphics_safety_unlock(reinterpret_cast<void *>(static_cast<uintptr_t>(lock)));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeGraphicsSafety_nativeRecover(JNIEnv *env, jobject, jstring jroot)
{
	auto root = copy_string(env, jroot);
	// A file-only launcher operation must never acknowledge a live renderer's restore
	return root ? graphics_safety_recover(root.get(), 0) : 0;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_NativeGraphicsSafety_nativeRead(JNIEnv *env, jobject, jstring jroot)
{
	auto root = copy_string(env, jroot);
	graphics_safety_snapshot snapshot;
	if (!root || !graphics_safety_read_staged(root.get(), nullptr, &snapshot)) return nullptr;
	jintArray result = env->NewIntArray(GRAPHICS_SAFE_FIELD_COUNT);
	if (result) env->SetIntArrayRegion(result, 0, GRAPHICS_SAFE_FIELD_COUNT, snapshot.values);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeGraphicsSafety_nativeStage(JNIEnv *env, jobject, jstring jroot,
                                                       jobjectArray jkeys, jintArray jvalues)
try {
	auto root = copy_string(env, jroot);
	if (!root || !jkeys || !jvalues) return 0;
	const auto count = env->GetArrayLength(jkeys);
	if (count <= 0 || count > GRAPHICS_SAFE_FIELD_COUNT || env->GetArrayLength(jvalues) != count) return 0;
	jint values[GRAPHICS_SAFE_FIELD_COUNT];
	env->GetIntArrayRegion(jvalues, 0, count, values);
	if (env->ExceptionCheck()) return 0;
	std::vector<std::string> keys;
	keys.reserve(count);
	for (int i = 0; i < count; ++i) {
		auto key = static_cast<jstring>(env->GetObjectArrayElement(jkeys, i));
		auto text = copy_string(env, key);
		if (key) env->DeleteLocalRef(key);
		if (!text || env->ExceptionCheck()) return 0;
		keys.emplace_back(text.get());
	}
	graphics_config_update updates[GRAPHICS_SAFE_FIELD_COUNT];
	for (int i = 0; i < count; ++i) updates[i] = { keys[i].c_str(), values[i] };
	if (!graphics_safety_recover(root.get(), 0)) return 0;
	return graphics_safety_stage(root.get(), updates, static_cast<size_t>(count));
} catch (...) {
	return 0;
}
