#!/bin/bash
# Antigravity OS — Clean build artifacts
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Cleaning build outputs, virtual ESP, and image files..."
rm -rf "$ROOT_DIR/build"
rm -rf "$ROOT_DIR/ESP"
rm -f "$ROOT_DIR/esp.img"

echo "Clean completed!"
