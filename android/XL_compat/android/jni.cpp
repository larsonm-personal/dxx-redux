#include "../compatibility.h"
#include <jni.h>
#include <stdexcept>

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

extern "C" JNIEXPORT jstring JNICALL Java_com_dxxredux_app_XlCompatibility_nativeInspectPackage(JNIEnv *env, jobject, jstring owner, jstring descriptor, jobjectArray paths, jobjectArray hashes)
{
	const auto count = env->GetArrayLength(paths);
	if (count > 4096 || count != env->GetArrayLength(hashes)) return nullptr;
	auto string_value = [env](jstring value) {
		if (!value) throw std::runtime_error("Missing package string");
		const char *utf = env->GetStringUTFChars(value, nullptr);
		if (!utf) throw std::runtime_error("Cannot read package string");
		const std::string result(utf);
		env->ReleaseStringUTFChars(value, utf);
		return result;
	};
	try {
		std::vector<xl_compat::PackageResource> resources;
		for (jsize i = 0; i < count; ++i) {
			auto path = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
			auto hash = static_cast<jstring>(env->GetObjectArrayElement(hashes, i));
			resources.push_back({ string_value(path), string_value(hash) });
			env->DeleteLocalRef(path);
			env->DeleteLocalRef(hash);
		}
		const auto report = xl_compat::package_compatibility_json(string_value(owner), string_value(descriptor), resources);
		return env->NewStringUTF(report.c_str());
	} catch (...) {
		return nullptr;
	}
}
