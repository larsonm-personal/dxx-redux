#!/system/bin/sh
# Owned disposable-emulator fixture; never called by production code
set -eu
chain=${3:-}
case "$chain" in DXX_T_????????) ;; *) exit 2 ;; esac
case "$chain" in *[!A-Za-z0-9_]*) exit 2 ;; esac
operation=${1:-}
uid=${2:-}
case "$uid" in '' | *[!0-9]*) exit 2 ;; esac
[ "$uid" -ge 10000 ] || exit 2
case "$operation" in
install)
    # Refuse pre-existing state rather than assuming ownership
    iptables -N "$chain"
    rollback() {
        iptables -D OUTPUT -p udp -m owner --uid-owner "$uid" -j "$chain" 2>/dev/null || true
        iptables -F "$chain"
        iptables -X "$chain"
    }
    trap rollback EXIT
    iptables -A "$chain" -d 127.0.0.0/8 -j RETURN
    iptables -A "$chain" -d 10.0.2.2 -p udp -m multiport --dports 3478,3479,9001 -j RETURN
    iptables -A "$chain" -j DROP
    iptables -I OUTPUT -p udp -m owner --uid-owner "$uid" -j "$chain"
    trap - EXIT
    ;;
count)
    iptables -L "$chain" -v -x -n | awk '$3 == "DROP" {print $1}'
    ;;
remove)
    if ! iptables -L "$chain" -n >/dev/null 2>&1; then exit 0; fi
    iptables -D OUTPUT -p udp -m owner --uid-owner "$uid" -j "$chain"
    iptables -F "$chain"
    iptables -X "$chain"
    ;;
*) exit 2 ;;
esac
