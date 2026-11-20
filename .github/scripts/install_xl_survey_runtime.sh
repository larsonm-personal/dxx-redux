#!/usr/bin/env bash
set -euo pipefail

# Ubuntu 24.04 runtime packages for the ordinary D2 readiness executable
packages=(libphysfs1 libsdl1.2debian libsdl-mixer1.2 libpng16-16t64 libglew2.2 libopengl0 libglu1-mesa zlib1g)
missing=()
for package in "${packages[@]}"; do
    if [[ $(dpkg-query -W -f='${Status}' "$package" 2>/dev/null) != "install ok installed" ]]; then
        missing+=("$package")
    fi
done
if [[ ! -x /usr/bin/7z ]]; then missing+=(p7zip-full); fi

if ((${#missing[@]})); then
    bash "$(dirname -- "${BASH_SOURCE[0]}")/install_noble_packages.sh" "${missing[@]}"
fi
/usr/bin/7z i >/dev/null
