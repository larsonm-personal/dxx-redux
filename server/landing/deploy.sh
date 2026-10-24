#!/usr/bin/env bash
# Render and atomically publish the landing page on the VPS
set -euo pipefail
LANDING_DIR=$(dirname -- "$(readlink -f -- "$0")")
html=''
if [[ ${1:-} == --html ]]; then
    html=${2:?Expected a rendered HTML path after --html}
    shift 2
    config=''
    webroot=${1:-/var/www/dxx-landing}
else
    config=${1:-$LANDING_DIR/config.local.json}
    webroot=${2:-/var/www/dxx-landing}
fi
work=$(mktemp -d)
staged=''
cleanup() {
    rm -rf -- "$work"
    if [[ -n $staged ]]; then sudo rm -f -- "$staged"; fi
}
trap cleanup EXIT
if [[ -n $html ]]; then
    cp -- "$html" "$work/index.html"
else
    pwsh -NoProfile -File "$LANDING_DIR/build.ps1" -ConfigPath "$config" -OutputDirectory "$work"
fi
test -s "$work/index.html"
sudo install -d -m 755 -- "$webroot"
target="$webroot/index.html"
if sudo test ! -L "$target" && sudo cmp -s -- "$work/index.html" "$target"; then
    echo 'Landing page already up to date'
    exit 0
fi
staged=$(sudo mktemp "$webroot/.index.XXXXXXXX")
sudo install -m 644 -- "$work/index.html" "$staged"
sudo mv -Tf -- "$staged" "$target"
staged=''
echo 'Landing page deployed'
