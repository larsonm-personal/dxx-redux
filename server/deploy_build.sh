#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== dxx-matchmaking: build and deploy ==="
echo "Server directory: $SCRIPT_DIR"
echo ""

# --- 0. Deployment mode ---
echo "Select deployment mode:"
echo "  1) LAN  -- self-signed TLS, no nginx, direct connections on port 9000"
echo "  2) Web  -- nginx reverse proxy with Let's Encrypt TLS on port 443"
echo ""
while true; do
    read -p "Enter 1 or 2: " MODE_CHOICE
    case "$MODE_CHOICE" in
        1) DEPLOY_MODE="lan"; break ;;
        2) DEPLOY_MODE="web"; break ;;
        *) echo "Invalid choice" ;;
    esac
done
echo "Mode: $DEPLOY_MODE"
echo ""

# --- 1. Git pull ---
echo "--- Updating repository ---"
cd "$SCRIPT_DIR/.."
git pull
cd "$SCRIPT_DIR"
echo ""

# --- 2. Rust toolchain ---
echo "--- Checking Rust toolchain ---"
if command -v rustup &>/dev/null; then
    echo "rustup found, updating stable toolchain..."
    rustup update stable
else
    echo "Rust not found. Installing via rustup..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable
    # Source the environment so cargo is available in this session
    . "$HOME/.cargo/env"
fi
rustc --version
cargo --version
echo ""

# --- 3. Build release binary ---
echo "--- Building release binary ---"
cargo build --release
BINARY="$SCRIPT_DIR/target/release/dxx-matchmaking"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: binary not found at $BINARY"
    exit 1
fi
echo "Binary built: $BINARY"
echo ""

# --- 4. Config file ---
CONFIG_FILE="$SCRIPT_DIR/server_config.json5"
if [ "$DEPLOY_MODE" = "lan" ]; then
    TEMPLATE_CONFIG="$SCRIPT_DIR/config.json5.lan"
else
    TEMPLATE_CONFIG="$SCRIPT_DIR/config.json5.default"
fi

if [ ! -f "$TEMPLATE_CONFIG" ]; then
    echo "WARNING: $TEMPLATE_CONFIG not found (should be in repo)"
fi

if [ -f "$CONFIG_FILE" ]; then
    echo "Config file already exists: $CONFIG_FILE"
    read -p "Overwrite with $DEPLOY_MODE template? (y/N): " answer
    if [[ "$answer" == [Yy] ]]; then
        cp "$TEMPLATE_CONFIG" "$CONFIG_FILE"
        echo "Config overwritten from $DEPLOY_MODE template"
    else
        echo "Keeping existing config"
    fi
else
    echo "No config file found. Creating from $DEPLOY_MODE template..."
    cp "$TEMPLATE_CONFIG" "$CONFIG_FILE"
    echo "Created $CONFIG_FILE"
fi
echo ""

# --- 5. Mode-specific setup ---
if [ "$DEPLOY_MODE" = "lan" ]; then
    # --- LAN: generate self-signed cert ---
    echo "--- LAN: generating self-signed TLS certificate ---"
    CERT_SCRIPT="$SCRIPT_DIR/generate_lan_cert.sh"
    if [ ! -f "$CERT_SCRIPT" ]; then
        echo "ERROR: $CERT_SCRIPT not found"
        exit 1
    fi
    if [ -f "$SCRIPT_DIR/lan_certs/cert.pem" ]; then
        echo "Existing LAN cert found"
        read -p "Regenerate? (y/N): " answer
        if [[ "$answer" == [Yy] ]]; then
            bash "$CERT_SCRIPT"
        else
            echo "Keeping existing cert"
        fi
    else
        bash "$CERT_SCRIPT"
    fi
    echo ""
else
    # --- Web: log directory + nginx ---
    LOG_DIR="/var/log/dxx-matchmaking"
    if [ ! -d "$LOG_DIR" ]; then
        echo "Creating log directory: $LOG_DIR"
        sudo mkdir -p "$LOG_DIR"
        sudo chown "$(whoami):$(whoami)" "$LOG_DIR"
    fi

    echo "--- Web: nginx reverse proxy ---"
    NGINX_CONF="/etc/nginx/sites-available/dxx-matchmaking"
    NGINX_TEMPLATE="$SCRIPT_DIR/nginx-dxx-matchmaking.conf"

    if ! command -v nginx &>/dev/null; then
        echo "nginx not found. Installing..."
        sudo apt-get update -qq
        sudo apt-get install -y -qq nginx
    fi

    if [ -f "$NGINX_CONF" ]; then
        echo "nginx config already exists: $NGINX_CONF"
        read -p "Overwrite? (y/N): " answer
        if [[ "$answer" != [Yy] ]]; then
            echo "Keeping existing nginx config"
        else
            read -p "Enter your domain name (e.g. match.example.com): " DOMAIN
            if [ -z "$DOMAIN" ]; then
                echo "ERROR: domain name required for TLS"
                exit 1
            fi
            sed "s/DOMAIN/$DOMAIN/g" "$NGINX_TEMPLATE" | sudo tee "$NGINX_CONF" > /dev/null
            echo "nginx config written"
        fi
    else
        read -p "Enter your domain name (e.g. match.example.com): " DOMAIN
        if [ -z "$DOMAIN" ]; then
            echo "ERROR: domain name required for TLS"
            exit 1
        fi
        sed "s/DOMAIN/$DOMAIN/g" "$NGINX_TEMPLATE" | sudo tee "$NGINX_CONF" > /dev/null
        sudo ln -sf "$NGINX_CONF" /etc/nginx/sites-enabled/dxx-matchmaking
        echo "nginx config installed and enabled"
    fi

    sudo nginx -t
    sudo systemctl reload nginx
    echo "nginx reloaded"
    echo ""
fi

# --- 6. Public address configuration ---
if [ "$DEPLOY_MODE" = "web" ]; then
    echo "--- Configuring public addresses (relay + STUN) ---"

    # Detect public IP for reference
    PUBLIC_IP=""
    for url in "https://api.ipify.org" "https://ifconfig.me" "https://icanhazip.com"; do
        PUBLIC_IP=$(curl -s --max-time 5 "$url" 2>/dev/null | tr -d '[:space:]')
        if [[ "$PUBLIC_IP" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            break
        fi
        PUBLIC_IP=""
    done

    if [ -n "$PUBLIC_IP" ]; then
        echo "Detected public IP: $PUBLIC_IP"
    else
        echo "Could not auto-detect public IP"
    fi

    # Read current config values (uncommented lines only)
    CURRENT_RELAY=$(grep -oP '^\s*relay_public_addr:\s*"\K[^"]+' "$CONFIG_FILE" 2>/dev/null || echo "")
    CURRENT_STUN=$(grep -oP '^\s*stun_public_addrs:\s*"\K[^"]+' "$CONFIG_FILE" 2>/dev/null || echo "")
    CURRENT_HOST=""
    if [ -n "$CURRENT_RELAY" ]; then
        CURRENT_HOST="${CURRENT_RELAY%:*}"
    fi

    # Pick the best default: nginx domain > existing config > detected IP
    SUGGESTED=""
    if [ -n "$DOMAIN" ]; then
        SUGGESTED="$DOMAIN"
    elif [ -n "$CURRENT_HOST" ] && [ "$CURRENT_HOST" != "YOUR_HOST" ]; then
        SUGGESTED="$CURRENT_HOST"
    elif [ -n "$PUBLIC_IP" ]; then
        SUGGESTED="$PUBLIC_IP"
    fi

    echo ""
    echo "The relay and STUN services need a public hostname or IP that clients"
    echo "can reach. A domain name is recommended for flexibility"
    if [ -n "$CURRENT_HOST" ] && [ "$CURRENT_HOST" != "YOUR_HOST" ]; then
        echo "  Current config: $CURRENT_HOST"
    fi
    if [ -n "$PUBLIC_IP" ] && [ "$PUBLIC_IP" != "$CURRENT_HOST" ]; then
        echo "  Detected IP:    $PUBLIC_IP"
    fi
    if [ -n "$DOMAIN" ] && [ "$DOMAIN" != "$CURRENT_HOST" ]; then
        echo "  nginx domain:   $DOMAIN"
    fi
    echo ""

    if [ -n "$SUGGESTED" ]; then
        read -p "Public host [$SUGGESTED]: " USER_HOST
        PUBLIC_HOST="${USER_HOST:-$SUGGESTED}"
    else
        read -p "Public host (domain or IP): " PUBLIC_HOST
    fi

    if [ -z "$PUBLIC_HOST" ]; then
        echo "Skipping -- you can edit $CONFIG_FILE manually later"
    else
        WANT_RELAY="${PUBLIC_HOST}:9001"
        WANT_STUN="${PUBLIC_HOST}:3478,${PUBLIC_HOST}:3479"

        if [ "$CURRENT_RELAY" = "$WANT_RELAY" ] && [ "$CURRENT_STUN" = "$WANT_STUN" ]; then
            echo "Config already has correct values. No changes needed"
        else
            echo "Will set:"
            echo "  relay_public_addr: \"$WANT_RELAY\""
            echo "  stun_public_addrs: \"$WANT_STUN\""
            read -p "Write to $CONFIG_FILE? (Y/n): " WRITE_ANSWER
            if [[ "$WRITE_ANSWER" != [Nn] ]]; then
                # Patch relay_public_addr (handles both commented-out and active lines)
                if grep -qE '^\s*(//\s*)?relay_public_addr:' "$CONFIG_FILE"; then
                    sed -i -E 's|^\s*(//\s*)?relay_public_addr:.*|    relay_public_addr: "'"$WANT_RELAY"'",|' "$CONFIG_FILE"
                else
                    sed -i '/^}/i\    relay_public_addr: "'"$WANT_RELAY"'",' "$CONFIG_FILE"
                fi
                # Patch stun_public_addrs
                if grep -qE '^\s*(//\s*)?stun_public_addrs:' "$CONFIG_FILE"; then
                    sed -i -E 's|^\s*(//\s*)?stun_public_addrs:.*|    stun_public_addrs: "'"$WANT_STUN"'",|' "$CONFIG_FILE"
                else
                    sed -i '/^}/i\    stun_public_addrs: "'"$WANT_STUN"'",' "$CONFIG_FILE"
                fi
                echo "Config updated"
            else
                echo "Skipped. Edit $CONFIG_FILE manually"
            fi
        fi
    fi
    echo ""
fi

# --- 7. Install and activate systemd service ---
echo "--- Installing systemd service ---"
"$SCRIPT_DIR/deploy_service.sh"

echo ""
echo "=== Deploy complete ($DEPLOY_MODE mode) ==="
echo "  Binary:  $BINARY"
echo "  Config:  $CONFIG_FILE"
echo "  Service: systemctl status dxx-matchmaking"

if [ "$DEPLOY_MODE" = "lan" ]; then
    LAN_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
    echo "  Certs:   $SCRIPT_DIR/lan_certs/"
    echo ""
    echo "Clients connect via: wss://${LAN_IP:-YOUR_LAN_IP}:9000/ws"
    echo ""
    echo "Next steps:"
    echo "  1. Verify config: $CONFIG_FILE"
    echo "  2. sudo systemctl restart dxx-matchmaking"
else
    echo "  Logs:    /var/log/dxx-matchmaking + journalctl -u dxx-matchmaking"
    echo "  nginx:   /etc/nginx/sites-available/dxx-matchmaking"
    echo ""
    echo "Clients connect via: wss://${DOMAIN:-YOUR_DOMAIN}/ws"
    echo ""
    echo "Next steps:"
    echo "  1. Set up Let's Encrypt: sudo certbot --nginx -d ${DOMAIN:-YOUR_DOMAIN}"
    echo "  2. sudo systemctl restart dxx-matchmaking"
fi
