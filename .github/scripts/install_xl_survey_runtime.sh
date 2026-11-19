#!/usr/bin/env bash
set -euo pipefail

# Ubuntu 24.04 packages for the ordinary D2 readiness executable and archive reader
packages=(libphysfs1 libsdl1.2debian libsdl-mixer1.2 libpng16-16t64 libglew2.2 libopengl0 libglu1-mesa zlib1g p7zip-full)

case "${1:-}" in
prepare)
    [[ $# == 2 ]]
    bundle=$(realpath -m -- "$2")
    cache=$(mktemp -d)
    chmod 755 "$cache"
    # APT populates this new cache from the signed Noble sources and includes
    # dependencies missing on the stock runner. No game executable runs here
    bash "$(dirname -- "${BASH_SOURCE[0]}")/install_noble_packages.sh" \
        --download-only --reinstall --no-remove -o "Dir::Cache::archives=$cache" "${packages[@]}"
    (cd "$cache" && tar -cf "$bundle" -- *.deb)
    ;;
install)
    [[ $# == 3 && $3 =~ ^[0-9a-f]{64}$ ]]
    bundle=$(realpath -- "$2")
    actual=$(sha256sum <"$bundle")
    [[ ${actual%% *} == "$3" ]] || {
        echo 'Runtime bundle SHA-256 mismatch' >&2
        exit 1
    }
    cache=$(mktemp -d)
    chmod 755 "$cache"
    tar --extract --file "$bundle" --directory "$cache" --no-same-owner
    sudo apt-get -o "Dir::Cache::archives=$cache" install --no-download --no-remove --no-install-recommends -y "$cache"/*.deb
    for package in "${packages[@]}"; do
        [[ $(dpkg-query -W -f='${Status}' "$package") == 'install ok installed' ]]
    done
    /usr/bin/7z i >/dev/null
    ;;
*)
    echo 'Usage: install_xl_survey_runtime.sh prepare BUNDLE | install BUNDLE SHA256' >&2
    exit 2
    ;;
esac
