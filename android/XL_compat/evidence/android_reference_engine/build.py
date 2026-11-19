"""Cross-build the pinned original XL engine as an isolated Android reference executable."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PINS = {
    "dep-curl": "fdb8a789d2b446b77bd7cdd2eff95f6cbc814cf4",
    "dep-glu": "a2b96c7bba8db8fec3e02fb4227a7f7b02cabad1",
    "dep-SDL_image": "2ffb2e3e1eba037897164e3ac6c67570d8bccd79",
    "dep-SDL_mixer": "1a14d94ed4271e45435ecb5512d61792e1a42932",
    "dep-SDL_net": "7aa8ffae32fda6455792c5538a8b64a655dd0721",
}
GL4ES = "8b1a3da5d2331d0f88b70be25963182d46eed857"


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def pinned(source, commit):
    head = subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD"], text=True).strip()
    if head != commit:
        raise ValueError(f"{source} requires {commit}, found {head}")
    run("git", "-C", source, "diff", "--quiet", "HEAD", "--")


def digest(path):
    with path.open("rb") as fp:
        return hashlib.file_digest(fp, "sha256").hexdigest()


def source_tree_digest(root):
    checksum = hashlib.sha256()
    files = sorted((p for p in root.rglob("*") if p.is_file()), key=lambda p: p.relative_to(root).as_posix())
    for path in files:
        entry = digest(path) + "  " + path.relative_to(root).as_posix() + "\n"
        checksum.update(entry.encode())
    return checksum.hexdigest()


def prepare_shutdown(observer, upstream):
    # The desktop observer exits from the live frame. On Android the engine must
    # release level graphics and sound while its EGL context still exists.
    header = upstream / "main/campaign_reference.h"
    observer.replace_once(
        header,
        "void xl_reference_frame()",
        """static int campaign_result = 25;
bool xl_reference_finished() { return campaign_result != 25; }

void xl_reference_frame()""",
    )
    observer.replace_once(
        header,
        "std::exit(failed ? 23 : 0);",
        """campaign_receipt = nullptr;
    campaign_result = failed ? 23 : 0;
    SetFunctionMode(FMODE_EXIT);""",
    )
    observer.replace_once(header, "MainLoop();\n\treturn 25;", "MainLoop();\n\treturn campaign_result;")

    release_mouse = "\t\t\tGrabMouse (0, 1);\n\t\t\tpaletteManager.EnableEffect (true);"
    observer.replace_once(
        upstream / "main/descent.cpp",
        release_mouse,
        release_mouse
        + """
            extern bool xl_reference_finished ();
            if (xl_reference_finished ()) { SetFunctionMode (FMODE_EXIT); break; }""",
    )
    observer.replace_once(
        upstream / "main/descent.cpp",
        "if (campaign_requested ()) return campaign_reference ();",
        """if (campaign_requested ()) {
    const int result = campaign_reference ();
    CleanUp ();
    CloseGame ();
    extern void xl_reference_release_drawable ();
    xl_reference_release_drawable ();
    return result;
}""",
    )
    observer.replace_once(
        upstream / "main/game.cpp",
        "xl_reference_frame ();\n\n\tPROF_CONT",
        "xl_reference_frame ();\n\textern bool xl_reference_finished ();\n\tif (xl_reference_finished ()) break;\n\n\tPROF_CONT",
    )


def build(a):
    repo = a.redux.resolve()
    sys.path.insert(0, str(repo / "android/tests"))
    import prepare_xl_campaign_reference as observer

    work = a.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    upstream = work / "original"
    deps = a.deps.resolve()
    for name, commit in PINS.items():
        pinned(deps / name, commit)
    pinned(a.gl_source, GL4ES)
    source_archives = {
        "GLEW 2.2.0": (a.glew, "90733a013f8ba195f1c4e4f7433fc66b3225af1d82cd82e077738fb2c903c111"),
        "libpng 1.6.50": (a.png, "0712efe1bdff7a1457abfc9177f4fe485d77010cc8204aba693eb49b87e8844c"),
    }
    for name, (path, expected) in source_archives.items():
        if source_tree_digest(path) != expected:
            raise ValueError(f"Unexpected extracted source tree for {name}")
    run(sys.executable, repo / "android/tests/prepare_xl_campaign_reference.py", a.original, upstream)
    run("git", "-C", upstream, "apply", "--unidiff-zero", "--ignore-space-change", HERE / "android.patch")
    entry = upstream / "main/descent.cpp"
    observer.replace_once(
        entry, "signal (SIGABRT, D2SignalHandler);", "#ifndef __ANDROID__\nsignal (SIGABRT, D2SignalHandler);"
    )
    observer.replace_once(entry, "signal (SIGTERM, D2SignalHandler);", "signal (SIGTERM, D2SignalHandler);\n#endif")
    observer.replace_once(
        upstream / "main/campaign_reference.h",
        "std::fclose(campaign_receipt);",
        "extern void xl_reference_write_presented();\n\txl_reference_write_presented();\n\tstd::fclose(campaign_receipt);",
    )

    prepare_shutdown(observer, upstream)

    # Reuse the original engine's source lists and relative include directories.
    original_cmake = upstream / "CMakeLists.txt"
    text = original_cmake.read_text()
    sources = text[text.index("#local includes") : text.index("add_executable(d2x-xl ")]
    sources = sources.replace('if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")', "if(UNIX)")
    original_cmake.write_text(
        """cmake_minimum_required(VERSION 3.20)
project(XLAndroid LANGUAGES C CXX)
add_compile_definitions(RELEASE HAVE_STRUCT_TIMESPEC SDL_INPUT USE_SDL_MIXER USE_SDL_IMAGE GLEW_NO_GLU GLEW_EGL)
add_compile_options(-fsigned-char -Wno-format-security -Wno-parentheses-equality)
include_directories("${XL_HEADERS}" "${XL_HEADERS}/SDL" "${XL_HEADERS}/GL" "${XL_GL_SOURCE}/include")
"""
        + sources
        + """
list(REMOVE_ITEM d2x-xl_sources network/linux/ipx_bsd.cpp)
add_library(xl_original_core STATIC ${d2x-xl_sources})
"""
    )
    headers = work / "headers"
    sdl = headers / "SDL"
    sdl.mkdir(parents=True)
    for header in (a.sdl_source / "include").glob("*.h"):
        shutil.copyfile(header, sdl / header.name)
    shutil.copyfile(repo / "android/app/src/main/cpp/SDL_config_android.h", sdl / "SDL_config.h")
    for name in ("SDL_image", "SDL_mixer", "SDL_net"):
        shutil.copyfile(deps / ("dep-" + name) / (name + ".h"), sdl / (name + ".h"))
    (headers / "GL").mkdir()
    shutil.copyfile(a.glew / "include/GL/glew.h", headers / "GL/glew.h")
    shutil.copytree(deps / "dep-curl/include/curl", headers / "curl")
    flags = {
        "CMAKE_TOOLCHAIN_FILE": a.ndk.resolve() / "build/cmake/android.toolchain.cmake",
        "CMAKE_MAKE_PROGRAM": a.ninja.resolve(),
        "ANDROID_ABI": a.abi,
        "ANDROID_PLATFORM": "android-28",
        "ANDROID_STL": "c++_static",
        "CMAKE_BUILD_TYPE": "Release",
        "XL_HEADERS": headers,
        "XL_ORIGINAL": upstream,
        "XL_GLEW": a.glew.resolve(),
        "XL_PNG": a.png.resolve(),
        "XL_DEPS": deps,
        "XL_GL_SOURCE": a.gl_source.resolve(),
        "XL_GL_LIBRARY": a.gl_library.resolve(),
        "XL_SDL_LIBRARY": a.sdl_library.resolve(),
    }
    config = work / "build"
    run(
        a.cmake,
        "-S",
        HERE,
        "-B",
        config,
        "-G",
        "Ninja",
        *(f"-D{k}={v.as_posix() if isinstance(v, Path) else v}" for k, v in flags.items()),
    )
    run(a.cmake, "--build", config, "--parallel", a.jobs, "--target", "xl_original")
    binaries = {"xl_original": config / "xl_original", "libGL.so.1": a.gl_library, "libSDL12.so": a.sdl_library}
    for name, path in binaries.items():
        shutil.copyfile(path, work / name)
    receipt = {
        "abi": a.abi,
        "original": observer.REVISION,
        "gl4es": GL4ES,
        "dependencies": PINS,
        "source_archives": {name: value for name, (_, value) in source_archives.items()},
        "binary_sha256": {name: digest(work / name) for name in binaries},
        "source_lf_sha256": {
            str(path.relative_to(HERE)): hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()
            for path in (
                HERE / "android.patch",
                HERE / "CMakeLists.txt",
                HERE / "egl_sdl.cpp",
                Path(__file__).resolve(),
            )
        },
        "headers_sha256": {
            "glew.h": digest(headers / "GL/glew.h"),
            "png.h": digest(a.png / "png.h"),
            "SDL_config.h": digest(sdl / "SDL_config.h"),
        },
    }
    (work / "build-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in (
        "work",
        "redux",
        "original",
        "deps",
        "gl-source",
        "gl-library",
        "sdl-source",
        "sdl-library",
        "glew",
        "png",
        "ndk",
        "cmake",
        "ninja",
    ):
        parser.add_argument("--" + flag, type=Path, required=True)
    parser.add_argument("--abi", choices=("arm64-v8a", "x86_64"), required=True)
    parser.add_argument("--jobs", type=int, default=4)
    build(parser.parse_args())
