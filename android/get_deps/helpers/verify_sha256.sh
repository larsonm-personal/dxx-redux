#!/bin/bash

verify_sha256() {
    local path="$1"
    local expected="$2"
    local label="${3:-$path}"
    local actual

    if [[ ! "$expected" =~ ^[0-9a-fA-F]{64}$ ]]; then
        echo "ERROR: Invalid configured SHA-256 for $label" >&2
        return 1
    fi
    if [ ! -f "$path" ]; then
        echo "ERROR: $label not found at $path" >&2
        return 1
    fi
    if command -v sha256sum >/dev/null 2>&1; then
        actual=$(sha256sum "$path" | awk '{print $1}')
    elif command -v shasum >/dev/null 2>&1; then
        actual=$(shasum -a 256 "$path" | awk '{print $1}')
    else
        echo "ERROR: sha256sum or shasum is required to verify $label" >&2
        return 1
    fi
    actual=$(printf '%s' "$actual" | tr 'A-F' 'a-f')
    expected=$(printf '%s' "$expected" | tr 'A-F' 'a-f')
    if [ "$actual" != "$expected" ]; then
        echo "ERROR: $label SHA-256 mismatch expected=$expected actual=$actual" >&2
        return 1
    fi
}
