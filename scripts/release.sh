#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_FILE="$ROOT_DIR/include/device_config.h"
ENV_NAME="${ENV:-lilygo-t-display-v1-1}"
BUILD_BIN="$ROOT_DIR/.pio/build/$ENV_NAME/firmware.bin"
DRY_RUN="${DRY_RUN:-0}"

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Error: required command '$1' not found."
    exit 1
  }
}

extract_define_value() {
  local name="$1"
  local value
  value="$(awk -v key="$name" '
    $1 == "#define" && $2 == key {
      if (match($0, /"[^"]*"/)) {
        print substr($0, RSTART + 1, RLENGTH - 2)
        exit
      }
    }
  ' "$CONFIG_FILE")"
  echo "$value"
}

require_cmd git
require_cmd make

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "Error: config file not found at $CONFIG_FILE"
  exit 1
fi

FW_VERSION="$(extract_define_value FW_VERSION)"
GITHUB_REPO="$(extract_define_value GITHUB_REPO)"
OTA_ASSET_NAME="$(extract_define_value OTA_ASSET_NAME)"

if [[ -z "$FW_VERSION" ]]; then
  echo "Error: FW_VERSION is empty in $CONFIG_FILE"
  exit 1
fi

if [[ -z "$GITHUB_REPO" ]]; then
  echo "Error: GITHUB_REPO is empty in $CONFIG_FILE"
  exit 1
fi

GITHUB_REPO="${GITHUB_REPO#/}"
if [[ "$GITHUB_REPO" != */* ]]; then
  echo "Error: GITHUB_REPO must be in owner/repo format."
  exit 1
fi

if [[ -z "$OTA_ASSET_NAME" ]]; then
  OTA_ASSET_NAME="firmware.bin"
fi

TAG="v$FW_VERSION"
if [[ "${FW_VERSION#v}" != "$FW_VERSION" ]]; then
  TAG="$FW_VERSION"
fi

echo ">>> Building firmware"
make -C "$ROOT_DIR" build ENV="$ENV_NAME"

if [[ ! -f "$BUILD_BIN" ]]; then
  echo "Error: build output not found: $BUILD_BIN"
  exit 1
fi

TMP_ASSET="$ROOT_DIR/.pio/build/$ENV_NAME/$OTA_ASSET_NAME"
if [[ "$TMP_ASSET" != "$BUILD_BIN" ]]; then
  cp "$BUILD_BIN" "$TMP_ASSET"
fi

if [[ "$DRY_RUN" == "1" ]]; then
  echo "DRY_RUN=1: checks and build completed, release was not created."
  echo "Tag to be created: $TAG"
  echo "Repo: $GITHUB_REPO"
  echo "Asset: $TMP_ASSET"
  exit 0
fi

require_cmd gh

if git -C "$ROOT_DIR" rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Error: git tag '$TAG' already exists locally."
  exit 1
fi

if git -C "$ROOT_DIR" ls-remote --tags origin "$TAG" | rg -q "$TAG"; then
  echo "Error: tag '$TAG' already exists on origin."
  exit 1
fi

echo ">>> Creating tag $TAG"
git -C "$ROOT_DIR" tag "$TAG"

echo ">>> Pushing tag to origin"
git -C "$ROOT_DIR" push origin "$TAG"

echo ">>> Creating GitHub release in $GITHUB_REPO"
gh release create "$TAG" \
  "$TMP_ASSET#$OTA_ASSET_NAME" \
  --repo "$GITHUB_REPO" \
  --title "$TAG" \
  --notes "Firmware release $TAG"

echo "Release completed: $TAG"
