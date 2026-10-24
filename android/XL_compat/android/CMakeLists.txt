add_library(xl_compat_metadata SHARED ../inventory.cpp ../compatibility.cpp jni.cpp)
target_compile_features(xl_compat_metadata PRIVATE cxx_std_17)
