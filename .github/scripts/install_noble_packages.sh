#!/usr/bin/env bash
set -euo pipefail

# Scope these Ubuntu 24.04 jobs to the signed archive without changing the runner's other package lists
sources="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/ubuntu-noble.sources"
apt=(sudo apt-get -o "Dir::Etc::sourcelist=$sources" -o Dir::Etc::sourceparts=- -o APT::Get::List-Cleanup=0
    -o Acquire::Retries=2 -o Acquire::https::Timeout=30)
"${apt[@]}" -o APT::Update::Error-Mode=any update
"${apt[@]}" install -y --no-install-recommends "$@"
