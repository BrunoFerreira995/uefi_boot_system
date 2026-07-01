#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "[Step 3] Launching the project..."

chmod +x "$SCRIPT_DIR/run.sh"
"$SCRIPT_DIR/run.sh"

echo "[Done] Project launch completed."
