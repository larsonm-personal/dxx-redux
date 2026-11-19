"""Build the isolated XL trace experiment from pinned source trees and an Android NDK."""

import argparse
import shutil
import subprocess
from pathlib import Path

APITRACE = "18cfd388ac47a9e00978b34d6c20341f3e2cdb56"
GL4ES = "8b1a3da5d2331d0f88b70be25963182d46eed857"
HERE = Path(__file__).resolve().parent


def run(*command):
    subprocess.run([str(arg) for arg in command], check=True)


def checkout(work, name, origin, commit, submodules=False):
    source = work / name
    if not source.exists():
        run("git", "init", "--quiet", source)
        run("git", "-C", source, "remote", "add", "origin", origin)
        run("git", "-C", source, "fetch", "--depth=1", "origin", commit)
        run("git", "-C", source, "checkout", "--detach", commit)
    identity = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if identity != commit:
        raise ValueError(f"{source} must contain {commit}, found {identity}")
    if submodules:
        run("git", "-C", source, "submodule", "update", "--init", "--recursive", "--depth=1")
    return source


def replace_once(path, original, replacement):
    current = path.read_text(encoding="utf-8")
    if current.count(replacement) == 1:
        return
    if current.count(original) != 1:
        raise ValueError(f"Expected a single pinned source location in {path}")
    path.write_text(current.replace(original, replacement), encoding="utf-8")


def build(args):
    work = args.work.resolve() / args.abi
    work.mkdir(parents=True, exist_ok=True)
    # GL4ES emits libGL in its source tree. Give each ABI independent sources.
    source = checkout(work, "apitrace", args.apitrace_source, APITRACE, submodules=True)
    gl = checkout(work, "gl4es", args.gl4es_source, GL4ES)
    replace_once(
        source / "CMakeLists.txt",
        'message (FATAL_ERROR "Android is no longer supported (https://git.io/vH2gW)")',
        'message (STATUS "Building the isolated offscreen Android retracer")',
    )
    wgl = source / "retrace/glretrace_wgl.cpp"
    definition = "static void retrace_wglSwapBuffers(trace::Call &call) {"
    replace_once(wgl, definition, "extern void xl_capture_swap(unsigned call);\n" + definition)
    swap = "    glws::Drawable *drawable = getDrawable(call.arg(0).toUIntPtr());\n\n    frame_complete(call);"
    replace_once(
        wgl, swap, swap.replace("    frame_complete(call);", "    xl_capture_swap(call.no);\n    frame_complete(call);")
    )
    shutil.copyfile(HERE / "egl_retrace.cpp", source / "retrace/xl_android_window.cpp")
    shutil.copyfile(HERE / "known_draw.cpp", source / "retrace/xl_known_draw.cpp")
    target = """
if(ANDROID)
    add_executable(xl_trace_replay xl_android_window.cpp)
    add_dependencies(xl_trace_replay glproc)
    target_link_libraries(xl_trace_replay retrace_common glretrace_common glhelpers glproc EGL GLESv2 dl log)
    add_executable(xl_known_draw xl_known_draw.cpp)
    target_link_libraries(xl_known_draw EGL GLESv2 dl)
endif()
"""
    cmake_list = source / "retrace/CMakeLists.txt"
    current = cmake_list.read_text(encoding="utf-8")
    if target not in current:
        cmake_list.write_text(current + target, encoding="utf-8")
    flags = [
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={args.ninja.resolve()}",
        f"-DCMAKE_TOOLCHAIN_FILE={args.ndk.resolve() / 'build/cmake/android.toolchain.cmake'}",
        f"-DANDROID_ABI={args.abi}",
        "-DANDROID_PLATFORM=android-28",
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    run(args.cmake, "-S", gl, "-B", work / "gl-build", *flags, "-DNOX11=ON")
    run(args.cmake, "--build", work / "gl-build", "--parallel", args.jobs)
    shutil.copyfile(gl / "lib/libGL.so.1", work / "libGL4ES.so")
    run(
        args.cmake,
        "-S",
        source,
        "-B",
        work / "trace-build",
        *flags,
        "-DBUILD_TESTING=OFF",
        "-DENABLE_GUI=OFF",
        "-DENABLE_CLI=OFF",
        "-DENABLE_X11=OFF",
        "-DENABLE_EGL=ON",
    )
    run(
        args.cmake,
        "--build",
        work / "trace-build",
        "--parallel",
        args.jobs,
        "--target",
        "xl_trace_replay",
        "xl_known_draw",
    )
    for executable in ("xl_trace_replay", "xl_known_draw"):
        shutil.copyfile(work / "trace-build" / executable, work / executable)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("work", "ndk", "cmake", "ninja"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--abi", required=True, choices=("x86_64", "arm64-v8a"))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--apitrace-source", default="https://github.com/apitrace/apitrace.git")
    parser.add_argument("--gl4es-source", default="https://github.com/ptitSeb/gl4es.git")
    build(parser.parse_args())
