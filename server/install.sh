#!/usr/bin/env bash
# Public Debian/Ubuntu installation; sourceable for hermetic orchestration tests
set -Eeuo pipefail

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

validate_settings() {
    [[ ${#DOMAIN} -le 253 && $DOMAIN == *.* && $DOMAIN =~ ^[a-z0-9]([a-z0-9.-]*[a-z0-9])?$ && $DOMAIN != *..* ]] || fail 'Use a lowercase DNS hostname, not a URL or IP'
    local label
    IFS=. read -ra labels <<<"$DOMAIN"
    for label in "${labels[@]}"; do
        [[ ${#label} -le 63 && $label != -* && $label != *- ]] || fail 'Invalid DNS label'
    done
    [[ $DOMAIN =~ [a-z] ]] || fail 'A DNS hostname is required'
    if [[ -n ${ACME_ACCOUNT:-} ]]; then
        [[ $ACME_ACCOUNT =~ ^[a-f0-9]{32}$ ]] || fail 'Invalid existing ACME account ID'
    else
        [[ $EMAIL =~ ^[a-zA-Z0-9._+%-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]] || fail 'Invalid certificate contact email'
    fi
    [[ ${#CLIENT_ID} -le 512 && $CLIENT_ID =~ ^[0-9]+-[a-zA-Z0-9_-]+\.apps\.googleusercontent\.com$ ]] || fail 'Invalid Google Web client ID'
}

# Snapshot each managed path once, including an old symlink itself, not its target
remember() {
    local target=$1 item
    for item in "${CHANGED[@]}"; do [[ $item != "$target" ]] || return 0; done
    local index=${#CHANGED[@]}
    CHANGED+=("$target")
    printf '%s %s\n' "$index" "$target" | sudo tee -a "$TX/paths" >/dev/null
    if sudo test -e "$target" || sudo test -L "$target"; then
        sudo cp -a -- "$target" "$TX/$index"
    fi
}

put_file() {
    local source=$1 target=$2 mode=$3
    if sudo test ! -L "$target" && sudo cmp -s -- "$source" "$target"; then
        if [[ $(sudo stat -c '%a' -- "$target") != "$mode" ]]; then
            remember "$target"
            sudo chmod "$mode" "$target"
        fi
        return
    fi
    remember "$target"
    sudo install -m "$mode" -- "$source" "$target.new"
    sudo mv -T -- "$target.new" "$target"
}

finish() {
    local status=$? index target
    trap - EXIT
    if ((status != 0)); then
        echo 'Install failed; restoring managed files (packages/certificates/firewall additions remain)' >&2
        set +e
        for ((index = ${#CHANGED[@]} - 1; index >= 0; index--)); do
            target=${CHANGED[index]}
            sudo rm -f -- "$target"
            if sudo test -e "$TX/$index" || sudo test -L "$TX/$index"; then
                sudo cp -a -- "$TX/$index" "$target"
            fi
        done
        sudo systemctl daemon-reload
        if [[ $WAS_ACTIVE == yes ]]; then
            sudo systemctl restart dxx-matchmaking || echo 'WARNING: old daemon failed to restart; inspect journalctl' >&2
        else
            sudo systemctl stop dxx-matchmaking
        fi
        if [[ $WAS_ENABLED != yes ]]; then sudo systemctl disable dxx-matchmaking; fi
        sudo nginx -t && sudo systemctl reload nginx
        echo "Recovery snapshots retained at $TX" >&2
    else
        echo "Previous managed files retained at $TX"
    fi
    exit "$status"
}

apply_install() {
    CHANGED=()
    WAS_ACTIVE=no
    WAS_ENABLED=no
    sudo systemctl is-active --quiet dxx-matchmaking && WAS_ACTIVE=yes
    sudo systemctl is-enabled --quiet dxx-matchmaking && WAS_ENABLED=yes
    TX=$(sudo mktemp -d "$SYSTEM_ROOT/var/backups/dxx-matchmaking/install.XXXXXXXX")
    trap finish EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    local site="$SYSTEM_ROOT/etc/nginx/sites-available/dxx-matchmaking"
    local enabled="$SYSTEM_ROOT/etc/nginx/sites-enabled/dxx-matchmaking"
    local cert="$SYSTEM_ROOT/etc/letsencrypt/live/$DOMAIN"
    local webroot="$SYSTEM_ROOT/var/www/dxx-acme"
    local state_dir="$SYSTEM_ROOT/var/lib/dxx-matchmaking"
    local config_file="$SYSTEM_ROOT/etc/dxx-matchmaking/server_config.jsonc"
    local binary_dir="$SYSTEM_ROOT/opt/dxx-matchmaking/bin"
    sudo install -d -m 755 "$webroot" "$SYSTEM_ROOT/etc/dxx-matchmaking" "$binary_dir" "$SYSTEM_ROOT/var/www/dxx-landing"
    sudo install -d -m 750 -o "$RUN_USER" "$SYSTEM_ROOT/var/log/dxx-matchmaking" "$state_dir"
    if ! sudo test -f "$config_file"; then
        # No existing settings or database are replaced
        put_file "$SERVER_DIR/config.default.jsonc" "$config_file" 640
        sudo chgrp "$RUN_USER" "$config_file"
    fi
    if sudo test -L "$enabled"; then
        [[ $(sudo readlink "$enabled") == "$site" ]] || fail 'Existing nginx site link points elsewhere; resolve it before installing'
    elif sudo test -e "$enabled"; then
        fail 'Existing enabled nginx site is not the expected symlink'
    else
        remember "$enabled"
        sudo ln -s "$site" "$enabled"
    fi

    # Bootstrap without referencing nonexistent certificates; reruns with certs keep TLS
    if ! sudo test -s "$cert/fullchain.pem" || ! sudo test -s "$cert/privkey.pem"; then
        cat >"$WORK/nginx" <<EOF
server {
    listen 80;
    listen [::]:80;
    server_name $DOMAIN;
    location /.well-known/acme-challenge/ { root $webroot; }
    location / { return 503; }
}
EOF
    else
        sed "s/DOMAIN/$DOMAIN/g" "$SERVER_DIR/nginx-dxx-matchmaking.conf" >"$WORK/nginx"
    fi
    put_file "$WORK/nginx" "$site" 644
    sudo nginx -t
    sudo systemctl enable --now nginx
    sudo systemctl reload nginx
    local account_args=(--email "$EMAIL")
    if [[ -n ${ACME_ACCOUNT:-} ]]; then account_args=(--account "$ACME_ACCOUNT"); fi
    sudo certbot certonly --webroot -w "$webroot" --cert-name "$DOMAIN" -d "$DOMAIN" \
        "${account_args[@]}" --agree-tos --non-interactive --keep-until-expiring
    if ! sudo test -s "$cert/fullchain.pem" || ! sudo test -s "$cert/privkey.pem"; then
        fail 'Certbot did not produce the expected certificate files'
    fi
    sed "s/DOMAIN/$DOMAIN/g" "$SERVER_DIR/nginx-dxx-matchmaking.conf" >"$WORK/nginx"
    put_file "$WORK/nginx" "$site" 644
    sudo nginx -t

    cat >"$WORK/environment" <<EOF
# Managed by server/install.sh; rerun with flags to change these values
DXX_DOMAIN=$DOMAIN
DXX_ACME_EMAIL=$EMAIL
DXX_ACME_ACCOUNT=${ACME_ACCOUNT:-}
GOOGLE_CLIENT_ID=$CLIENT_ID
GOOGLE_AUTH_ORIGIN=https://$DOMAIN:443
CONFIG_FILE=$config_file
WS_LISTEN_ADDR=127.0.0.1:9000
HTTP_LISTEN_ADDR=127.0.0.1:18080
ADMIN_HTTP_LISTEN_ADDR=127.0.0.1:18081
TRUSTED_PROXY_IPS=127.0.0.1
RELAY_LISTEN_ADDR=0.0.0.0:9001
STUN_LISTEN_ADDR=0.0.0.0:3478
STUN_LISTEN_ADDR_ALT=0.0.0.0:3479
RELAY_PUBLIC_ADDR=$DOMAIN:9001
STUN_PUBLIC_ADDRS=$DOMAIN:3478,$DOMAIN:3479
TLS_CERT_PATH=
TLS_KEY_PATH=
SKIP_GPGS_VERIFY=false
LOG_DIR=$SYSTEM_ROOT/var/log/dxx-matchmaking
EOF
    put_file "$WORK/environment" "$ENV_FILE" 600
    put_file "${BINARY_SOURCE:-$SERVER_DIR/target/install/release/dxx-matchmaking}" "$binary_dir/dxx-matchmaking" 755
    cat >"$WORK/service" <<EOF
[Unit]
Description=DXX matchmaking
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=60
StartLimitBurst=3

[Service]
Type=simple
User=$RUN_USER
WorkingDirectory=$state_dir
EnvironmentFile=$ENV_FILE
ExecStart=$binary_dir/dxx-matchmaking
Restart=on-failure
RestartSec=5
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$state_dir $SYSTEM_ROOT/var/log/dxx-matchmaking
PrivateTmp=true
PrivateDevices=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
RestrictSUIDSGID=true
RestrictNamespaces=true
LockPersonality=true
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
CapabilityBoundingSet=
AmbientCapabilities=
MemoryHigh=256M
MemoryMax=384M
MemorySwapMax=0
CPUQuota=50%
CPUWeight=10
IOWeight=10
TasksMax=128
LimitNOFILE=4096
LogRateLimitIntervalSec=30s
LogRateLimitBurst=200
UMask=0077

[Install]
WantedBy=multi-user.target
EOF
    put_file "$WORK/service" "$SYSTEM_ROOT/etc/systemd/system/dxx-matchmaking.service" 644
    sudo install -d -m 755 "$SYSTEM_ROOT/etc/letsencrypt/renewal-hooks/deploy"
    # Expand RENEWED_LINEAGE when Certbot runs the hook, not during installation
    # shellcheck disable=SC2016
    printf '#!/bin/sh\nset -eu\nif [ "${RENEWED_LINEAGE:-}" = "/etc/letsencrypt/live/%s" ]; then\n    nginx -t\n    systemctl reload nginx\nfi\n' "$DOMAIN" >"$WORK/renew-hook"
    put_file "$WORK/renew-hook" "$SYSTEM_ROOT/etc/letsencrypt/renewal-hooks/deploy/dxx-nginx" 755
    sudo systemctl daemon-reload
    if sudo systemctl cat snap.certbot.renew.timer >/dev/null 2>&1; then
        sudo systemctl enable --now snap.certbot.renew.timer
    else
        sudo systemctl enable --now certbot.timer
    fi
    sudo systemctl enable dxx-matchmaking
    sudo systemctl restart dxx-matchmaking
    sudo systemctl reload nginx
    # Local routing with real hostname/certificate verification; no insecure TLS bypass
    curl --fail --silent --show-error --noproxy '*' --retry 10 --retry-connrefused --retry-delay 2 \
        --retry-all-errors --max-time 5 --resolve "$DOMAIN:443:127.0.0.1" \
        "https://$DOMAIN/api/v1/ready" >/dev/null
    curl --fail --silent --show-error --noproxy '*' --max-time 10 \
        --resolve "$DOMAIN:443:127.0.0.1" -H 'Content-Type: application/json' \
        -d '{}' "https://$DOMAIN/api/v1/auth/challenge" >/dev/null
    sudo systemctl is-active --quiet dxx-matchmaking
    echo "Installed: wss://$DOMAIN/ws"
    echo 'Local TLS, listener readiness and Google challenge route passed; remote UDP and real Google login still need a client test'
}

main() {
    SERVER_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
    SYSTEM_ROOT=''
    ENV_FILE=/etc/dxx-matchmaking/environment
    RUN_USER=dxxmatch
    DOMAIN='' EMAIL='' CLIENT_ID='' ACCEPT=no ACME_ACCOUNT='' BINARY_SOURCE=''
    while (($#)); do
        case "$1" in
        --domain | --email | --google-client-id | --acme-account | --binary)
            (($# >= 2)) || fail "Missing value for $1"
            case "$1" in
            --domain) DOMAIN=$2 ;;
            --email) EMAIL=$2 ;;
            --google-client-id) CLIENT_ID=$2 ;;
            --acme-account) ACME_ACCOUNT=$2 ;;
            --binary) BINARY_SOURCE=$(readlink -f -- "$2") ;;
            esac
            shift 2
            ;;
        --accept-acme-terms)
            ACCEPT=yes
            shift
            ;;
        --help)
            echo 'Usage: bash server/install.sh [--domain HOST --email EMAIL --google-client-id ID --accept-acme-terms]'
            echo 'Optional: --binary /path/to/prebuilt-linux-binary --acme-account EXISTING_ID'
            echo 'Build as a non-root sudo user, or deploy a prebuilt binary as root; service runs as dxxmatch'
            return
            ;;
        *) fail "Unknown argument: $1" ;;
        esac
    done
    [[ $(id -u) != 0 || -n $BINARY_SOURCE ]] || fail 'Build as a non-root sudo user, or provide --binary when running as root'
    [[ -z $BINARY_SOURCE || -f $BINARY_SOURCE ]] || fail 'Prebuilt binary is missing'
    [[ $SERVER_DIR =~ ^/[a-zA-Z0-9_./-]+$ && $RUN_USER =~ ^[a-zA-Z_][a-zA-Z0-9_-]*$ ]] || fail 'Checkout/user path contains unsupported characters (including spaces)'
    [[ -d /run/systemd/system ]] || fail 'A systemd Linux host is required'
    # shellcheck source=/dev/null
    . /etc/os-release
    [[ $ID == ubuntu || $ID == debian ]] || fail 'This installer supports Debian and Ubuntu'
    local os_major=${VERSION_ID%%.*}
    [[ $os_major =~ ^[0-9]+$ ]] || fail 'Unrecognized OS version'
    if [[ $ID == ubuntu && $os_major -lt 22 ]] || [[ $ID == debian && $os_major -lt 12 ]]; then
        fail 'Use Ubuntu 22.04+ or Debian 12+'
    fi
    sudo -v
    # Never source writable saved configuration as shell code
    if sudo test -f "$ENV_FILE"; then
        [[ -n $DOMAIN ]] || DOMAIN=$(sudo sed -n 's/^DXX_DOMAIN=//p' "$ENV_FILE")
        [[ -n $EMAIL ]] || EMAIL=$(sudo sed -n 's/^DXX_ACME_EMAIL=//p' "$ENV_FILE")
        [[ -n $ACME_ACCOUNT ]] || ACME_ACCOUNT=$(sudo sed -n 's/^DXX_ACME_ACCOUNT=//p' "$ENV_FILE")
        [[ -n $CLIENT_ID ]] || CLIENT_ID=$(sudo sed -n 's/^GOOGLE_CLIENT_ID=//p' "$ENV_FILE")
        ACCEPT=yes
    fi
    [[ -n $DOMAIN ]] || read -r -p 'Public DNS hostname: ' DOMAIN
    [[ -n $EMAIL || -n $ACME_ACCOUNT ]] || read -r -p 'Certificate contact email: ' EMAIL
    [[ -n $CLIENT_ID ]] || read -r -p 'Google OAuth Web client ID (not secret): ' CLIENT_ID
    validate_settings
    if [[ $ACCEPT != yes ]]; then
        read -r -p "Accept Let's Encrypt subscriber terms (https://letsencrypt.org/repository/)? Type yes: " ACCEPT
        [[ $ACCEPT == yes ]] || fail 'Certificate terms were not accepted'
    fi
    local old_dir
    old_dir=$(sudo systemctl show dxx-matchmaking -p WorkingDirectory --value || true)
    [[ -z $old_dir || $old_dir == /var/lib/dxx-matchmaking ]] || fail 'Legacy service layout detected; migrate its config/database before changing the service'
    local old_user
    old_user=$(sudo systemctl show dxx-matchmaking -p User --value || true)
    [[ -z $old_user || $old_user == "$RUN_USER" ]] || fail 'Existing service uses a different account; explicit migration is required'
    [[ -z $(sudo systemctl show dxx-matchmaking -p DropInPaths --value) ]] || fail 'Review/remove existing systemd drop-ins before installing; they can override security settings'
    getent ahosts "$DOMAIN" >/dev/null || fail 'Hostname must resolve to this VPS before installation'
    echo 'Required provider firewall ports: TCP 80/443, UDP 3478/3479/9001; preserve your SSH port'
    echo 'Use DNS-only records pointing directly to this VPS (including any AAAA record)'
    if ! command -v nginx >/dev/null || ! command -v certbot >/dev/null || ! command -v curl >/dev/null; then
        sudo apt-get update
        sudo apt-get install -y ca-certificates curl nginx certbot python3-certbot-nginx
    fi
    if command -v ufw >/dev/null && sudo ufw status | grep -q '^Status: active'; then
        local port
        for port in 80/tcp 443/tcp 3478/udp 3479/udp 9001/udp; do sudo ufw allow "$port"; done
    fi
    WORK=$(mktemp -d)
    chmod 700 "$WORK"
    if [[ -z $BINARY_SOURCE ]]; then
        sudo apt-get update
        sudo apt-get install -y build-essential pkg-config cmake clang libclang-dev
        if ! command -v rustup >/dev/null; then
            curl --proto '=https' --tlsv1.2 --fail --show-error --silent https://sh.rustup.rs -o "$WORK/rustup.sh"
            sh "$WORK/rustup.sh" -y --profile minimal --default-toolchain none
        fi
        export PATH="$HOME/.cargo/bin:$PATH"
        local rust_version
        rust_version=$(awk '$1 == "rustc" {print $2}' "$SERVER_DIR/rust_version.txt")
        [[ $rust_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail 'Invalid pinned Rust version'
        rustup toolchain install "$rust_version" --profile minimal
        # Do not overwrite target/release: an older service may still execute that binary
        cargo "+$rust_version" build --locked --release --jobs 1 --manifest-path "$SERVER_DIR/Cargo.toml" --target-dir "$SERVER_DIR/target/install"
    fi
    if ! getent passwd "$RUN_USER" >/dev/null; then
        sudo useradd --system --user-group --home-dir /var/lib/dxx-matchmaking --create-home --shell /usr/sbin/nologin "$RUN_USER"
    fi
    [[ $(getent passwd "$RUN_USER" | cut -d: -f7) == /usr/sbin/nologin ]] || fail 'Service account unexpectedly has a login shell'
    sudo install -d -m 700 /var/backups/dxx-matchmaking
    # One activation transaction at a time across all checkouts
    sudo install -d -m 700 -o "$(id -un)" /var/lib/dxx-matchmaking-installer
    exec 9>/var/lib/dxx-matchmaking-installer/install.lock
    flock -n 9 || fail 'Another installer is active'
    apply_install
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then main "$@"; fi
