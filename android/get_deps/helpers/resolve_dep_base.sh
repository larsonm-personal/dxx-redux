#!/bin/bash
# resolve_dep_base.sh - Source this to set LOCAL_DIR from dependency_base.txt.
# Converts the Windows path in dependency_base.txt to a bash-compatible path.
# Usage: source "$(dirname "$0")/resolve_dep_base.sh"  (from get_deps/helpers/)
#    or: source "$(dirname "$0")/get_deps/helpers/resolve_dep_base.sh"  (from android/)

_resolve_dep_base() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    # shellcheck disable=SC1091
    source "$script_dir/platform.sh"
    local repo_root
    repo_root="$(cd "$script_dir/../../.." && pwd)"

    if [ ! -f "$repo_root/dependency_base.txt" ]; then
        local default_base="${DXX_DEPENDENCY_BASE:-$(get_default_dependency_base)}"
        printf '%s\n' "$default_base" >"$repo_root/dependency_base.txt"
        echo "Created dependency_base.txt with $default_base"
    fi

    # Read the first line, trim whitespace and carriage returns
    local raw
    raw="$(head -1 "$repo_root/dependency_base.txt" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"

    if [ -z "$raw" ]; then
        echo "ERROR: dependency_base.txt is empty" >&2
        return 1 2>/dev/null || exit 1
    fi

    # Convert Windows path to bash-compatible path
    # C:\local or C:/local -> try /c/local first, then /mnt/c/local, then C:/local
    local drive_letter dir_part
    if [[ "$raw" =~ ^([A-Za-z]):[/\\](.*) ]]; then
        drive_letter="${BASH_REMATCH[1],,}" # lowercase
        dir_part="${BASH_REMATCH[2]//\\//}" # backslash to forward slash
        # Remove trailing slash
        dir_part="${dir_part%/}"
        # Try MSYS/Git Bash style first
        if [ -d "/${drive_letter}/${dir_part}" ]; then
            LOCAL_DIR="/${drive_letter}/${dir_part}"
        elif [ -d "/mnt/${drive_letter}/${dir_part}" ]; then
            LOCAL_DIR="/mnt/${drive_letter}/${dir_part}"
        elif [ -d "${raw//\\//}" ]; then
            LOCAL_DIR="${raw//\\//}"
        else
            # Use the MSYS form even if it doesn't exist yet (install scripts create it)
            LOCAL_DIR="/${drive_letter}/${dir_part}"
        fi
    else
        # Already a Unix-style path
        LOCAL_DIR="$raw"
    fi

    mkdir -p "$LOCAL_DIR"
    export LOCAL_DIR
}

_resolve_dep_base
