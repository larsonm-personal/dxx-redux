#!/bin/bash
set -euxo pipefail

GIT_HASH=$(git rev-parse --short HEAD)

build_app() {
    builddir="$1"
    name="$2"
    prettyname="$3"

    zipfilename="${prettyname}-win-${GIT_HASH}.zip"
    outdir="${prettyname}"
    tmpdir="packagetemp"
    inipath="${name:0:2}/${name:0:3}-default.ini"

    # Have to bundle in separate directory because of case-insensitivity clashes
    mkdir ${tmpdir}
    cd ${tmpdir}

    mkdir "${outdir}"
    mkdir "${outdir}"/demos
    mkdir "${outdir}"/missions
    mkdir "${outdir}"/screenshots

    # Copy executable and libraris
    cp ../"${builddir}"/"${name}".exe "${outdir}"/
    # Include libraries built in-tree before resolving the executable's imports
    shopt -s nullglob
    for dll in ../"${builddir}"/*.dll; do cp -p "$dll" "${outdir}"/; done

    # Copy in .dlls the old fashioned way. This assumes all the needed DLLs are
    # ones from the mingw32/mingw64 installation
    ntldd -R "${outdir}"/"${name}".exe | grep -i "$MSYSTEM" | sort | awk '{print $3}' | while read -r dll; do cp -p "$(cygpath -u "${dll}")" "${outdir}"/; done

    # Copy the DLLS that SDL_mixer dynamically loads
    dlldir="/${MSYSTEM,,}/bin"
    for i in libvorbisfile-3.dll libvorbis-0.dll libogg-0.dll libmikmod-3.dll; do cp -p "$dlldir"/$i "$outdir"/; done
    if [ -f "$dlldir/libopenal-1.dll" ]; then cp -p "$dlldir/libopenal-1.dll" "$outdir"/; fi
    cp -p "$dlldir"/libFLAC.dll "$outdir"/libFLAC-8.dll

    # Copy in other resources
    cp ../COPYING.txt "${outdir}"/
    cp ../"${inipath}" "${outdir}"/

    # zip up and output to top level dir
    zip -r -X ../"${zipfilename}" "${outdir}"

    cd ..

    rm -rf ${tmpdir}
}

build_app "buildd1/main" "d1x-redux" "D1X-Redux"
build_app "buildd2/main" "d2x-redux" "D2X-Redux"

# Clean up
