#!/bin/bash
set -euxo pipefail

export MACOSX_DEPLOYMENT_TARGET=10.14

version=$(git tag --points-at HEAD)
if [ -n "$version" ]; then
    if [ "${version:0:1}" = "v" ]; then
        version=${version:1}
    fi
else
    version=$(git rev-parse --short HEAD)
fi

find_lib() {
    exe="$1"
    lib="$2"
    dirlist=$(otool -l "$exe" | awk '/^ *cmd LC_RPATH/{rp=1} /^Load command/{rp=0} rp&&/^ *path /{print $2}')
    while IFS= read -r dir; do
        if [ -f "$dir/$lib" ]; then
            echo "$dir/$lib"
            return 0
        fi
    done <<<"$dirlist"
    return 1
}

build_app() {
    builddir="$1"
    name="$2"
    prettyname="$3"
    appltag="$4"
    srcdir="${name:0:2}"
    contents="${prettyname}.app/Contents"

    zipfilename="${name}-${version}-mac.zip"

    mkdir -p "$contents"/MacOS
    mkdir -p "$contents"/Resources
    mkdir -p "$contents"/libs
    cp -p "$builddir"/"$name" "$contents"/MacOS
    cp -p "$srcdir"/arch/cocoa/Info.plist "$contents"
    cp -p "$srcdir"/arch/cocoa/"${name}".icns "$contents"/Resources
    echo -n "APPL${appltag}" >"$contents"/PkgInfo

    dylibbundler -ns -od -b -x "$contents"/MacOS/"$name" -d "$contents"/libs \
        -s "$builddir"/../_deps/sdl_mixer-1.2-cmake-build

    # SDL2 is loaded dynamically by sdl12-compat
    sdl2=libSDL2-2.0.0.dylib
    sdl2_source=$(find_lib "$builddir/$name" "$sdl2" || true)
    if [ -z "$sdl2_source" ]; then sdl2_source="$(brew --prefix sdl2)/lib/$sdl2"; fi
    cp -p "$sdl2_source" "$contents"/libs
    dylibbundler -ns -of -b -x "$contents"/libs/$sdl2 -d "$contents"/libs

    find "$contents"/libs -name '*.dylib' -exec codesign -f -s - '{}' \;
    codesign -f -s - "$contents"/MacOS/"$name"

    # zip up and output to top level dir
    zip -r -X "${zipfilename}" "${prettyname}".app
}

build_app "buildd1/main" "d1x-redux" "D1X-Redux" "DCNT"
build_app "buildd2/main" "d2x-redux" "D2X-Redux" "DCT2"

# Clean up
