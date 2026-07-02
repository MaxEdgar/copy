#!/usr/bin/env bash
set -euo pipefail

REPO="MaxEdgar/copy"
NAME="copy"
INSTALL_PATH="/usr/local/bin/$NAME"

echo "Installing $NAME..."

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "$ARCH" in
  x86_64|amd64)
    ARCH="amd64"
    ;;
  arm64|aarch64)
    ARCH="arm64"
    ;;
  *)
    echo "Unsupported architecture: $ARCH"
    exit 1
    ;;
esac

case "$OS" in
  linux)
    ASSET="${NAME}-linux-${ARCH}"
    ;;
  darwin)
    ASSET="${NAME}-macos-${ARCH}"
    ;;
  *)
    echo "Unsupported OS: $OS"
    exit 1
    ;;
esac

URL="https://github.com/${REPO}/releases/latest/download/${ASSET}"

echo "Detected: $OS $ARCH"
echo "Downloading: $URL"

TMP_FILE="$(mktemp)"

curl -fsSL "$URL" -o "$TMP_FILE"

if [[ ! -s "$TMP_FILE" ]]; then
  echo "Download failed"
  exit 1
fi

chmod +x "$TMP_FILE"

# ensure install dir exists (safety for unusual systems)
if [[ ! -d "/usr/local/bin" ]]; then
  echo "/usr/local/bin does not exist"
  exit 1
fi

echo "Installing to $INSTALL_PATH"

if [[ -f "$INSTALL_PATH" ]]; then
  echo "Existing version found, updating..."
fi

if [[ -w "/usr/local/bin" ]]; then
  mv -f "$TMP_FILE" "$INSTALL_PATH"
else
  sudo mv -f "$TMP_FILE" "$INSTALL_PATH"
fi

echo "Installed successfully"
echo "Version:"
"$INSTALL_PATH" --version || true
