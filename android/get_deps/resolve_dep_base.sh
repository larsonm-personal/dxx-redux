#!/bin/bash
# resolve_dep_base.sh - Source this to set LOCAL_DIR from dependency_base.txt.
# Converts the Windows path in dependency_base.txt to a bash-compatible path.
# Usage: source "$(dirname "$0")/resolve_dep_base.sh"  (from get_deps/)
#    or: source "$(dirname "$0")/get_deps/resolve_dep_base.sh"  (from android/)

_resolve_dep_base() {
    # Find the repo root by looking for dependency_base.txt
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local repo_root="$script_dir"
    # Walk up until we find dependency_base.txt (max 5 levels)
    for _ in 1 2 3 4 5; do
        if [ -f "$repo_root/dependency_base.txt" ]; then
            break
        fi
        repo_root="$(dirname "$repo_root")"
    done

    if [ ! -f "$repo_root/dependency_base.txt" ]; then
        echo "ERROR: dependency_base.txt not found." >&2
        echo "Create it in the repo root with a single line containing the path to your" >&2
        echo "dependency directory (e.g. C:\\local)." >&2
        return 1 2>/dev/null || exit 1
    fi

    # Read the first line, trim whitespace and carriage returns
    local raw
    raw="$(head -1 "$repo_root/dependency_base.txt" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"

    if [ -z "$raw" ]; then
        echo "ERROR: dependency_base.txt is empty." >&2
        return 1 2>/dev/null || exit 1
    fi

    # Convert Windows path to bash-compatible path
    # C:\local or C:/local -> try /c/local first, then /mnt/c/local, then C:/local
    local drive_letter dir_part
    if [[ "$raw" =~ ^([A-Za-z]):[/\\](.*) ]]; then
        drive_letter="${BASH_REMATCH[1],,}"  # lowercase
        dir_part="${BASH_REMATCH[2]//\\//}"  # backslash to forward slash
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

    export LOCAL_DIR
}

_resolve_dep_base
