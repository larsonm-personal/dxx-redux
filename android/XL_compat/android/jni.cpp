#include "../compatibility.h"
#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL Java_com_dxxredux_app_XlCompatibility_nativeInspect(JNIEnv *env, jobject, jstring name, jbyteArray data)
{
	const auto size = env->GetArrayLength(data);
	if (size > 64 * 1024 * 1024) return nullptr;
	const char *utf = env->GetStringUTFChars(name, nullptr);
	if (!utf) return nullptr;
	const std::string source(utf);
	env->ReleaseStringUTFChars(name, utf);
	try {
		xl_compat::Bytes bytes(static_cast<std::size_t>(size));
		env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte *>(bytes.data()));
		if (env->ExceptionCheck()) return nullptr;
		const auto report = xl_compat::compatibility_json(source, bytes);
		return env->NewStringUTF(report.c_str());
	} catch (...) {
		return nullptr;
	}
}
