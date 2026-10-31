#!/usr/bin/env bash
# Download prebuilt libriag archives from GitHub releases or workflow artifacts.
set -euo pipefail

LIB_KIND="${RIAG_LIB_KIND:-shared}"
LINUX_VARIANT="${RIAG_LINUX_VARIANT:-compat}"
REPOSITORY="${RIAG_GITHUB_REPOSITORY:-cntSafety/RiaGraph}"
RUN_ID="${RIAG_PRECOMPILED_RUN_ID:-}"
VERSION_OVERRIDE="${RIAG_VERSION:-}"

if [ "$LIB_KIND" != "shared" ] && [ "$LIB_KIND" != "static" ]; then
  echo "Unsupported RIAG_LIB_KIND: $LIB_KIND (expected 'shared' or 'static')" >&2
  exit 1
fi

if [ "$LINUX_VARIANT" != "compat" ] && [ "$LINUX_VARIANT" != "perf" ]; then
  echo "Unsupported RIAG_LINUX_VARIANT: $LINUX_VARIANT (expected 'compat' or 'perf')" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="${RIAG_TARGET_DIR:-$PROJECT_DIR/lib}"

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Darwin)
    if [ "$ARCH" = "x86_64" ]; then
      ARCHIVE_ARCH="x86_64"
    elif [ "$ARCH" = "arm64" ]; then
      ARCHIVE_ARCH="arm64"
    else
      echo "Unsupported macOS architecture: $ARCH" >&2
      exit 1
    fi
    if [ "$LIB_KIND" = "static" ]; then
      ARCHIVE="libriag-static-osx-${ARCHIVE_ARCH}.tar.gz"
      ARTIFACT_NAME="libriag-static-osx-${ARCHIVE_ARCH}"
      LIB_NAME="libriag.a"
    else
      ARCHIVE="libriag-osx-${ARCHIVE_ARCH}.tar.gz"
      ARTIFACT_NAME="libriag-osx-${ARCHIVE_ARCH}"
      LIB_NAME="libriag.dylib"
    fi
    ;;
  Linux)
    if [ "$ARCH" = "x86_64" ]; then
      ARCHIVE_ARCH="x86_64"
    elif [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
      ARCHIVE_ARCH="aarch64"
    else
      echo "Unsupported Linux architecture: $ARCH" >&2
      exit 1
    fi
    if [ "$LIB_KIND" = "static" ]; then
      ARCHIVE="libriag-static-linux-${ARCHIVE_ARCH}-${LINUX_VARIANT}.tar.gz"
      ARTIFACT_NAME="libriag-static-linux-${ARCHIVE_ARCH}-${LINUX_VARIANT}"
      LIB_NAME="libriag.a"
    else
      ARCHIVE="libriag-linux-${ARCHIVE_ARCH}.tar.gz"
      ARTIFACT_NAME="libriag-linux-${ARCHIVE_ARCH}"
      LIB_NAME="libriag.so"
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    if [ "$ARCH" = "x86_64" ] || [ "$ARCH" = "AMD64" ]; then
      ARCHIVE_ARCH="x86_64"
    else
      echo "Unsupported Windows architecture: $ARCH" >&2
      exit 1
    fi
    if [ "$LIB_KIND" = "static" ]; then
      ARCHIVE="libriag-static-windows-${ARCHIVE_ARCH}.zip"
      ARTIFACT_NAME="libriag-static-windows-${ARCHIVE_ARCH}"
      LIB_NAME="riag.lib"
    else
      ARCHIVE="libriag-windows-${ARCHIVE_ARCH}.zip"
      ARTIFACT_NAME="libriag-windows-${ARCHIVE_ARCH}"
      LIB_NAME="riag_shared.dll"
    fi
    ;;
  *)
    echo "Unsupported OS: $OS" >&2
    exit 1
    ;;
esac

if [ -f "$TARGET_DIR/$LIB_NAME" ]; then
  echo "libriag already exists in $TARGET_DIR"
  exit 0
fi

mkdir -p "$TARGET_DIR"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

fetch_release_archive() {
  local version
  if [ -n "$VERSION_OVERRIDE" ]; then
    version="$VERSION_OVERRIDE"
  else
    version="$(curl -sS "https://api.github.com/repos/${REPOSITORY}/releases/latest" | grep -o '"tag_name": "v\([^"]*\)"' | cut -d'"' -f4 | cut -c2-)"
  fi
  local download_url="https://github.com/${REPOSITORY}/releases/download/v${version}/${ARCHIVE}"
  curl -fSL "$download_url" -o "$TMPDIR/$ARCHIVE"
  echo "release:v${version}"
}

fetch_run_artifact() {
  if ! command -v gh >/dev/null 2>&1; then
    echo "gh CLI is required when RIAG_PRECOMPILED_RUN_ID is set" >&2
    exit 1
  fi
  gh run download "$RUN_ID" --repo "$REPOSITORY" --name "$ARTIFACT_NAME" --dir "$TMPDIR/artifact" >/dev/null
  local extracted_archive
  extracted_archive="$(find "$TMPDIR/artifact" -type f -name "$ARCHIVE" | head -n1)"
  if [ -z "$extracted_archive" ]; then
    echo "Artifact ${ARTIFACT_NAME} does not contain ${ARCHIVE}" >&2
    exit 1
  fi
  mv "$extracted_archive" "$TMPDIR/$ARCHIVE"
  echo "run:${RUN_ID}/${ARTIFACT_NAME}"
}

if [ -n "$RUN_ID" ]; then
  SOURCE_DESC="$(fetch_run_artifact)"
else
  SOURCE_DESC="$(fetch_release_archive)"
fi

if [ "${ARCHIVE##*.}" = "zip" ]; then
  unzip -o "$TMPDIR/$ARCHIVE" -d "$TARGET_DIR"
else
  tar xzf "$TMPDIR/$ARCHIVE" -C "$TARGET_DIR"
fi

echo "Installed ${ARCHIVE} from ${SOURCE_DESC} to $TARGET_DIR"
