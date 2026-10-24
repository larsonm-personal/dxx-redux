#!/usr/bin/env bash
# Hermetic activation tests: real filesystem operations, mocked external services
# Globals and command functions are consumed by the sourced installer
# shellcheck disable=SC2034,SC2329
set -Eeuo pipefail
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf -- "$TEST_ROOT"' EXIT

run_case() (
    # shellcheck source=../install.sh
    source "$SOURCE/install.sh"
    SYSTEM_ROOT="$TEST_ROOT/$1"
    SERVER_DIR="$SYSTEM_ROOT/repo/server"
    ENV_FILE="$SYSTEM_ROOT/etc/dxx-matchmaking/environment"
    WORK="$SYSTEM_ROOT/work"
    RUN_USER=$(id -un)
    STATE_DIR="$SYSTEM_ROOT/var/lib/dxx-matchmaking"
    CONFIG_FILE="$SYSTEM_ROOT/etc/dxx-matchmaking/server_config.jsonc"
    DOMAIN=match.example.com EMAIL=operator@example.com CLIENT_ID=123456-test.apps.googleusercontent.com
    CASE=$1
    if [[ $CASE == existing_account ]]; then
        EMAIL=''
        ACME_ACCOUNT=0123456789abcdef0123456789abcdef
    fi
    mkdir -p "$SERVER_DIR/target/install/release" "$WORK" "$SYSTEM_ROOT/etc/nginx/sites-available" \
        "$SYSTEM_ROOT/etc/nginx/sites-enabled" "$SYSTEM_ROOT/etc/systemd/system" "$SYSTEM_ROOT/var/backups/dxx-matchmaking"
    cp "$SOURCE/config.default.jsonc" "$SOURCE/nginx-dxx-matchmaking.conf" "$SERVER_DIR/"
    printf 'new binary\n' >"$SERVER_DIR/target/install/release/dxx-matchmaking"
    mkdir -p "$STATE_DIR" "$SYSTEM_ROOT/etc/dxx-matchmaking" "$SYSTEM_ROOT/var/www/dxx-landing"
    if [[ $CASE != first ]]; then printf 'preserve custom config\n' >"$CONFIG_FILE"; fi
    printf 'preserve database\n' >"$STATE_DIR/dxx_matchmaking.db"
    printf 'preserve landing\n' >"$SYSTEM_ROOT/var/www/dxx-landing/index.html"
    if [[ $CASE != first ]]; then
        mkdir -p "$SYSTEM_ROOT/opt/dxx-matchmaking/bin"
        printf 'old binary\n' >"$SYSTEM_ROOT/opt/dxx-matchmaking/bin/dxx-matchmaking"
        printf 'old nginx\n' >"$SYSTEM_ROOT/etc/nginx/sites-available/dxx-matchmaking"
        touch "$SYSTEM_ROOT/active" "$SYSTEM_ROOT/enabled"
        mkdir -p "$SYSTEM_ROOT/etc/dxx-matchmaking"
        printf 'previous environment\n' >"$ENV_FILE"
        printf 'previous unit\n' >"$SYSTEM_ROOT/etc/systemd/system/dxx-matchmaking.service"
    fi
    sudo() { "$@"; }
    systemctl() {
        printf 'systemctl %s\n' "$*" >>"$SYSTEM_ROOT/events"
        case "$1:$*" in
        is-active:*)
            test -f "$SYSTEM_ROOT/active"
            return
            ;;
        is-enabled:*)
            test -f "$SYSTEM_ROOT/enabled"
            return
            ;;
        restart:*dxx-matchmaking*)
            if [[ $CASE == daemon_failure && ! -f $SYSTEM_ROOT/failed_once ]]; then
                touch "$SYSTEM_ROOT/failed_once"
                return 1
            fi
            touch "$SYSTEM_ROOT/active"
            ;;
        stop:*dxx-matchmaking*) rm -f "$SYSTEM_ROOT/active" ;;
        enable:*dxx-matchmaking*) touch "$SYSTEM_ROOT/enabled" ;;
        disable:*dxx-matchmaking*) rm -f "$SYSTEM_ROOT/enabled" ;;
        esac
        return 0
    }
    nginx() {
        printf 'nginx\n' >>"$SYSTEM_ROOT/events"
        [[ $CASE != nginx_failure ]] || return 1
        if grep -q 'ssl_certificate' "$SYSTEM_ROOT/etc/nginx/sites-available/dxx-matchmaking"; then
            [[ $CASE != tls_failure ]] || return 1
            [[ -s $SYSTEM_ROOT/etc/letsencrypt/live/$DOMAIN/fullchain.pem ]] || return 1
        fi
    }
    certbot() {
        printf 'certbot\n' >>"$SYSTEM_ROOT/events"
        [[ $CASE != cert_failure ]] || return 1
        [[ $CASE != missing_cert ]] || return 0
        [[ $* == *--keep-until-expiring* && $* == *--webroot* ]] || return 1
        if [[ $CASE == existing_account ]]; then
            [[ $* == *--account* && $* != *--email* ]] || return 1
        fi
        mkdir -p "$SYSTEM_ROOT/etc/letsencrypt/live/$DOMAIN"
        printf 'certificate\n' >"$SYSTEM_ROOT/etc/letsencrypt/live/$DOMAIN/fullchain.pem"
        printf 'key\n' >"$SYSTEM_ROOT/etc/letsencrypt/live/$DOMAIN/privkey.pem"
    }
    curl() {
        printf 'curl %s\n' "$*" >>"$SYSTEM_ROOT/events"
        [[ $* != *--insecure* && $* == *--resolve* ]] || return 1
        [[ $CASE != readiness_failure ]] || return 1
        if [[ $CASE == auth_failure && $* == */auth/challenge* ]]; then return 1; fi
    }
    validate_settings
    apply_install
    if [[ $CASE == rerun ]]; then
        cp "$ENV_FILE" "$WORK/first-env"
        cp "$SYSTEM_ROOT/etc/nginx/sites-available/dxx-matchmaking" "$WORK/first-nginx"
        apply_install
        cmp "$ENV_FILE" "$WORK/first-env"
        cmp "$SYSTEM_ROOT/etc/nginx/sites-available/dxx-matchmaking" "$WORK/first-nginx"
    fi
)

for scenario in first rerun existing_account cert_failure missing_cert nginx_failure tls_failure readiness_failure auth_failure daemon_failure; do
    set +e
    # Not an if/|| context, which would disable errexit inside the function
    (
        set -e
        run_case "$scenario"
    ) >"$TEST_ROOT/$scenario.log" 2>&1
    result=$?
    set -e
    root="$TEST_ROOT/$scenario"
    if [[ $scenario == first || $scenario == rerun || $scenario == existing_account ]]; then
        [[ $result == 0 ]] || {
            cat "$TEST_ROOT/$scenario.log"
            exit 1
        }
        grep -q '^GOOGLE_AUTH_ORIGIN=https://match.example.com:443$' "$root/etc/dxx-matchmaking/environment"
        grep -q '^SKIP_GPGS_VERIFY=false$' "$root/etc/dxx-matchmaking/environment"
        grep -q '^ADMIN_HTTP_LISTEN_ADDR=127.0.0.1:18081$' "$root/etc/dxx-matchmaking/environment"
        grep -q '^HTTP_LISTEN_ADDR=127.0.0.1:18080$' "$root/etc/dxx-matchmaking/environment"
        grep -q '127.0.0.1:18080/api/' "$root/etc/nginx/sites-available/dxx-matchmaking"
        grep -q '^ProtectHome=true$' "$root/etc/systemd/system/dxx-matchmaking.service"
        [[ -x $root/etc/letsencrypt/renewal-hooks/deploy/dxx-nginx ]]
    else
        [[ $result != 0 ]] || {
            echo "Expected failure: $scenario"
            exit 1
        }
        grep -q '^old binary$' "$root/opt/dxx-matchmaking/bin/dxx-matchmaking"
        grep -q '^old nginx$' "$root/etc/nginx/sites-available/dxx-matchmaking"
        grep -q '^previous environment$' "$root/etc/dxx-matchmaking/environment"
        grep -q '^previous unit$' "$root/etc/systemd/system/dxx-matchmaking.service"
        [[ -f $root/active && -f $root/enabled ]]
    fi
    if [[ $scenario == first ]]; then
        cmp "$SOURCE/config.default.jsonc" "$root/etc/dxx-matchmaking/server_config.jsonc"
    else
        grep -q '^preserve custom config$' "$root/etc/dxx-matchmaking/server_config.jsonc"
    fi
    grep -q '^preserve database$' "$root/var/lib/dxx-matchmaking/dxx_matchmaking.db"
    grep -q '^preserve landing$' "$root/var/www/dxx-landing/index.html"
    echo "PASS $scenario"
done

for invalid in 'bad;command.example' '../etc/passwd' 'https://match.example.com' 'bad..example.com'; do
    if (
        source "$SOURCE/install.sh"
        DOMAIN=$invalid EMAIL=a@example.com CLIENT_ID=123-test.apps.googleusercontent.com
        validate_settings
    ) 2>/dev/null; then
        echo 'Invalid hostname accepted'
        exit 1
    fi
done
echo 'PASS hostname injection rejection'

# Identical bytes can still need permission repair; rollback must restore metadata
for scenario in mode_repair mode_rollback; do
    root="$TEST_ROOT/$scenario"
    mkdir -p "$root/backup"
    printf 'unchanged content\n' >"$root/source"
    cp "$root/source" "$root/target"
    chmod 644 "$root/target"
    set +e
    (
        set -e
        source "$SOURCE/install.sh"
        sudo() { "$@"; }
        systemctl() { return 0; }
        nginx() { return 0; }
        CHANGED=() TX="$root/backup" WAS_ACTIVE=no WAS_ENABLED=no
        trap finish EXIT
        put_file "$root/source" "$root/target" 600
        [[ $(stat -c '%a' "$root/target") == 600 ]]
        [[ ${#CHANGED[@]} == 1 ]]
        put_file "$root/source" "$root/target" 600
        [[ ${#CHANGED[@]} == 1 ]]
        if [[ $scenario == mode_rollback ]]; then exit 1; fi
    ) >"$root/output" 2>&1
    result=$?
    set -e
    if [[ $scenario == mode_repair ]]; then
        [[ $result == 0 && $(stat -c '%a' "$root/target") == 600 ]]
    else
        [[ $result == 1 && $(stat -c '%a' "$root/target") == 644 ]]
    fi
    cmp "$root/source" "$root/target"
    echo "PASS $scenario"
done
