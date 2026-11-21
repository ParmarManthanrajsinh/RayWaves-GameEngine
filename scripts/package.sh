#!/usr/bin/env bash
# Forward to packaging/package.sh. Mirrors scripts/package.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec "$ROOT/packaging/package.sh" "$@"
