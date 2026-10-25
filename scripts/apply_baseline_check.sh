#!/usr/bin/env bash
# Wrapper around tests/validate_source_invariants.py for local/CI convenience.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

usage() {
  cat <<'USAGE'
Usage: scripts/apply_baseline_check.sh [--help] [--root DIR]

Runs: python3 tests/validate_source_invariants.py --root <DIR>

Options:
  --help    Show this help and exit
  --root    Repository root (default: parent of scripts/)
USAGE
}

ROOT_ARG="$ROOT"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --root)
      ROOT_ARG="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found on PATH" >&2
  exit 127
fi

exec python3 "$ROOT/tests/validate_source_invariants.py" --root "$ROOT_ARG"
