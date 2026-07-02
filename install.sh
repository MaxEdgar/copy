#!/usr/bin/env bash
#
# install.sh - installer for copy
# https://github.com/MaxEdgar/copy
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/MaxEdgar/copy/main/install.sh | bash
#
set -Eeuo pipefail

REPO="MaxEdgar/copy"
NAME="copy"
INSTALL_DIR="/usr/local/bin"
INSTALL_PATH="${INSTALL_DIR}/${NAME}"
API_URL="https://api.github.com/repos/${REPO}/releases/latest"

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------
if [[ -t 1 ]]; then
  BOLD="$(printf '\033[1m')"
  DIM="$(printf '\033[2m')"
  RED="$(printf '\033[31m')"
  GREEN="$(printf '\033[32m')"
  YELLOW="$(printf '\033[33m')"
  BLUE="$(printf '\033[34m')"
  RESET="$(printf '\033[0m')"
else
  BOLD="" DIM="" RED="" GREEN="" YELLOW="" BLUE="" RESET=""
fi

info()    { printf "%s==>%s %s\n" "${BLUE}${BOLD}" "${RESET}" "$1"; }
success() { printf "%s✔%s %s\n" "${GREEN}${BOLD}" "${RESET}" "$1"; }
warn()    { printf "%s⚠%s %s\n" "${YELLOW}${BOLD}" "${RESET}" "$1"; }
error()   { printf "%s✘ %s%s\n" "${RED}${BOLD}" "$1" "${RESET}" >&2; }

TMP_DIR=""
cleanup() {
  [[ -n "$TMP_DIR" && -d "$TMP_DIR" ]] && rm -rf "$TMP_DIR"
}
trap cleanup EXIT
trap 'error "Installation failed on line $LINENO"' ERR

# ---------------------------------------------------------------------------
# Dependency checks
# ---------------------------------------------------------------------------
require() {
  command -v "$1" >/dev/null 2>&1 || {
    error "Required command not found: $1"
    exit 1
  }
}
require curl
require uname
require mktemp
require mv
require chmod

printf "%s%s copy installer%s\n" "$BOLD" "$NAME" "$RESET"
printf "%sinstalls the latest release of %s to %s%s\n\n" "$DIM" "$NAME" "$INSTALL_PATH" "$RESET"

# ---------------------------------------------------------------------------
# Detect OS / architecture
# ---------------------------------------------------------------------------
OS_RAW="$(uname -s)"
ARCH_RAW="$(uname -m)"
OS="$(printf '%s' "$OS_RAW" | tr '[:upper:]' '[:lower:]')"

case "$ARCH_RAW" in
  x86_64|amd64) ARCH="amd64" ;;
  arm64|aarch64) ARCH="arm64" ;;
  *)
    error "Unsupported architecture: $ARCH_RAW"
    exit 1
    ;;
esac

case "$OS" in
  linux)  ASSET="${NAME}-linux-${ARCH}" ;;
  darwin) ASSET="${NAME}-macos-${ARCH}" ;;
  *)
    error "Unsupported OS: $OS_RAW"
    exit 1
    ;;
esac

info "Detected platform: ${BOLD}${OS} ${ARCH}${RESET}"

# ---------------------------------------------------------------------------
# Resolve latest release tag
# ---------------------------------------------------------------------------
CURL="curl -fsSL --retry 3 --retry-delay 1 --connect-timeout 10"

info "Checking latest release..."
LATEST_TAG=""
if RELEASE_JSON="$($CURL "$API_URL" 2>/dev/null)"; then
  LATEST_TAG="$(printf '%s' "$RELEASE_JSON" | grep -m1 '"tag_name"' \
    | sed -E 's/.*"tag_name":[[:space:]]*"([^"]+)".*/\1/')"
fi

if [[ -n "$LATEST_TAG" ]]; then
  success "Latest version: ${BOLD}${LATEST_TAG}${RESET}"
  BASE_URL="https://github.com/${REPO}/releases/download/${LATEST_TAG}"
else
  warn "Could not query GitHub API, falling back to the latest-release redirect"
  BASE_URL="https://github.com/${REPO}/releases/latest/download"
fi

ASSET_URL="${BASE_URL}/${ASSET}"
CHECKSUMS_URL="${BASE_URL}/SHA256SUMS"

# ---------------------------------------------------------------------------
# Check existing installation
# ---------------------------------------------------------------------------
if command -v "$NAME" >/dev/null 2>&1; then
  CURRENT_VERSION="$("$NAME" --version 2>/dev/null || echo "unknown")"
  info "Existing installation found (${CURRENT_VERSION}), it will be updated"
fi

# ---------------------------------------------------------------------------
# Download
# ---------------------------------------------------------------------------
TMP_DIR="$(mktemp -d)"
TMP_FILE="${TMP_DIR}/${ASSET}"

info "Downloading ${ASSET}..."
if ! $CURL "$ASSET_URL" -o "$TMP_FILE"; then
  error "Download failed: $ASSET_URL"
  exit 1
fi

if [[ ! -s "$TMP_FILE" ]]; then
  error "Downloaded file is empty"
  exit 1
fi

# ---------------------------------------------------------------------------
# Verify checksum (best effort - does not fail the install if unavailable)
# ---------------------------------------------------------------------------
SHA_BIN=""
command -v sha256sum >/dev/null 2>&1 && SHA_BIN="sha256sum"
[[ -z "$SHA_BIN" ]] && command -v shasum >/dev/null 2>&1 && SHA_BIN="shasum -a 256"

if [[ -n "$SHA_BIN" ]]; then
  if $CURL "$CHECKSUMS_URL" -o "${TMP_DIR}/SHA256SUMS" 2>/dev/null; then
    EXPECTED="$(grep "$ASSET\$" "${TMP_DIR}/SHA256SUMS" | awk '{print $1}' || true)"
    ACTUAL="$($SHA_BIN "$TMP_FILE" | awk '{print $1}')"
    if [[ -n "$EXPECTED" && "$EXPECTED" == "$ACTUAL" ]]; then
      success "Checksum verified"
    elif [[ -n "$EXPECTED" ]]; then
      error "Checksum mismatch for $ASSET"
      exit 1
    else
      warn "No checksum entry found for $ASSET, skipping verification"
    fi
  else
    warn "SHA256SUMS unavailable, skipping checksum verification"
  fi
else
  warn "No sha256 tool found, skipping checksum verification"
fi

chmod +x "$TMP_FILE"

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
if [[ ! -d "$INSTALL_DIR" ]]; then
  error "$INSTALL_DIR does not exist"
  exit 1
fi

info "Installing to ${INSTALL_PATH}..."
if [[ -w "$INSTALL_DIR" ]]; then
  mv -f "$TMP_FILE" "$INSTALL_PATH"
else
  warn "Elevated permissions required for $INSTALL_DIR"
  sudo mv -f "$TMP_FILE" "$INSTALL_PATH"
fi

success "Installed to ${INSTALL_PATH}"

# ---------------------------------------------------------------------------
# Verify install
# ---------------------------------------------------------------------------
if INSTALLED_VERSION="$("$INSTALL_PATH" --version 2>/dev/null)"; then
  success "${NAME} ${INSTALLED_VERSION} is ready to use"
else
  warn "Installed, but could not run '${NAME} --version' to confirm"
fi
