#!/usr/bin/env bash
# Refresh only the runner UID grant approved for this disposable CI job.
set -euo pipefail

validate_nightly_kvm_acl() {
    local original=$1 current=$2 uid=$3
    [[ "$uid" =~ ^[0-9]+$ ]] || return 1
    # Original snapshot must remain a plain ACL; refresh may only retain/remove
    # our named user and the rw mask created by the initial grant.
    ! grep -Eq '^(mask::|user:[^:]+:|group:[^:]+:)' "$original" || return 1
    ! grep -Ev "^(#.*|[[:space:]]*|user::[rwx-]{3}|user:$uid:rw-|group::[rwx-]{3}|mask::rw-|other::[rwx-]{3})$" "$current" || return 1
    cmp -s "$original" <(sed -e "/^user:$uid:rw-$/d" -e '/^mask::rw-$/d' "$current")
}

refresh_nightly_kvm_access() {
    local original="$RUNNER_TEMP/dxx-kvm-before.acl"
    local marker="$RUNNER_TEMP/dxx-kvm-acl-changed"
    local current uid approved
    [[ -c /dev/kvm ]] || { echo 'KVM character device is absent' >&2; return 1; }
    if [[ ! -f "$marker" ]]; then
        # No prior mutation/snapshot: do not invent a refresh authorization.
        [[ -r /dev/kvm && -w /dev/kvm ]]
        return
    fi
    uid=$(id -u)
    approved=$(cat "$RUNNER_TEMP/dxx-kvm-runner.uid")
    [[ "$uid" == "$approved" ]] || { echo 'Runner UID changed; refusing KVM refresh' >&2; return 1; }
    current=$(mktemp "$RUNNER_TEMP/dxx-kvm-current.XXXXXX")
    getfacl -p -n /dev/kvm > "$current"
    if ! validate_nightly_kvm_acl "$original" "$current" "$uid"; then
        cat "$current"
        rm -f "$current"
        echo 'Unexpected KVM ACL or ownership; refusing refresh' >&2
        return 1
    fi
    rm -f "$current"
    if [[ ! -r /dev/kvm || ! -w /dev/kvm ]]; then
        echo "Approved KVM grant disappeared; refreshing existing runner UID $uid"
        sudo setfacl -m "u:$uid:rw" /dev/kvm
    fi
    [[ -r /dev/kvm && -w /dev/kvm ]]
    getfacl -p -n /dev/kvm
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    refresh_nightly_kvm_access
fi
