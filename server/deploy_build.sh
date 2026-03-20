#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== dxx-matchmaking: build and deploy ==="
echo "Server directory: $SCRIPT_DIR"
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
DEFAULT_CONFIG="$SCRIPT_DIR/config.json5.default"

if [ ! -f "$DEFAULT_CONFIG" ]; then
    echo "WARNING: $DEFAULT_CONFIG not found (should be in repo)"
fi

if [ -f "$CONFIG_FILE" ]; then
    echo "Config file already exists: $CONFIG_FILE"
    read -p "Overwrite with default config? (y/N): " answer
    if [[ "$answer" == [Yy] ]]; then
        cp "$DEFAULT_CONFIG" "$CONFIG_FILE"
        echo "Config overwritten from default."
    else
        echo "Keeping existing config."
    fi
else
    echo "No config file found. Creating from default..."
    cp "$DEFAULT_CONFIG" "$CONFIG_FILE"
    echo "Created $CONFIG_FILE -- edit it with your domain, IPs, and secrets."
fi
echo ""

# --- 5. Log directory ---
LOG_DIR="/var/log/dxx-matchmaking"
if [ ! -d "$LOG_DIR" ]; then
    echo "Creating log directory: $LOG_DIR"
    sudo mkdir -p "$LOG_DIR"
    sudo chown "$(whoami):$(whoami)" "$LOG_DIR"
fi
echo ""

# --- 6. Install and activate systemd service ---
echo "--- Installing systemd service ---"
"$SCRIPT_DIR/deploy_service.sh"

echo ""
echo "=== Deploy complete ==="
echo "  Binary:  $BINARY"
echo "  Config:  $CONFIG_FILE"
echo "  Logs:    $LOG_DIR (file) + journalctl -u dxx-matchmaking (systemd)"
echo "  Service: systemctl status dxx-matchmaking"
echo ""
echo "Next steps:"
echo "  1. Edit $CONFIG_FILE with your domain, public IP, secrets"
echo "  2. sudo systemctl restart dxx-matchmaking"
