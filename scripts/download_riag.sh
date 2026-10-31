#!/bin/sh
# Download prebuilt static libriag into the Rust crate cache.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ENV_FILE="${1:-$PROJECT_DIR/.cache/riag-prebuilt.env}"
CACHE_LIB_DIR="${RIAG_TARGET_DIR:-$PROJECT_DIR/.cache/riag-prebuilt/lib}"
LIB_KIND="${RIAG_LIB_KIND:-static}"
UPSTREAM_SCRIPT="$SCRIPT_DIR/download-libriag.sh"
UPSTREAM_URL="https://raw.githubusercontent.com/cntSafety/RiaGraph/refs/heads/main/scripts/download-libriag.sh"

if [ ! -f "$UPSTREAM_SCRIPT" ]; then
  echo "Fetching $UPSTREAM_URL ..."
  curl -fsSL "$UPSTREAM_URL" -o "$UPSTREAM_SCRIPT"
  chmod +x "$UPSTREAM_SCRIPT"
fi

RIAG_TARGET_DIR="$CACHE_LIB_DIR" RIAG_LIB_KIND="$LIB_KIND" bash "$UPSTREAM_SCRIPT"

OS="$(uname -s)"
if [ "$LIB_KIND" = "shared" ]; then
  case "$OS" in
    Darwin)
      LIB_PATH="$CACHE_LIB_DIR/libriag.dylib"
      ;;
    Linux)
      LIB_PATH="$CACHE_LIB_DIR/libriag.so"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      LIB_PATH="$CACHE_LIB_DIR/riag_shared.dll"
      ;;
    *)
      echo "Unsupported OS: $OS" >&2
      exit 1
      ;;
  esac
else
  case "$OS" in
    MINGW*|MSYS*|CYGWIN*)
      LIB_PATH="$CACHE_LIB_DIR/riag.lib"
      ;;
    *)
      LIB_PATH="$CACHE_LIB_DIR/libriag.a"
      ;;
  esac
fi

if [ ! -f "$LIB_PATH" ]; then
  echo "Expected precompiled library not found at $LIB_PATH" >&2
  exit 1
fi

mkdir -p "$(dirname "$ENV_FILE")"
cat > "$ENV_FILE" <<EOF
RIAG_LIBRARY_DIR=$CACHE_LIB_DIR
RIAG_INCLUDE_DIR=$CACHE_LIB_DIR
EOF

echo "Wrote $ENV_FILE"
echo "Resolved precompiled library: $LIB_PATH"
