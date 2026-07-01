#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "[Step 2] Building the project..."

chmod +x "$SCRIPT_DIR/build.sh"
"$SCRIPT_DIR/build.sh"

echo "[Done] Build completed successfully."
