#!/usr/bin/env bash

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target="${1:-}"

case "$target" in
  claude)
    default_dir="${HOME}/.claude/skills/design-slop"
    ;;
  codex)
    default_dir="${HOME}/.codex/skills/design-slop"
    ;;
  cursor)
    default_dir="${HOME}/.cursor/skills/design-slop"
    ;;
  -h|--help|"")
    echo "Usage: ./install.sh <claude|codex|cursor>"
    echo "Set DESIGN_SLOP_INSTALL_DIR to override the destination."
    exit 0
    ;;
  *)
    echo "Unknown target: ${target}" >&2
    echo "Use claude, codex, or cursor." >&2
    exit 1
    ;;
esac

install_dir="${DESIGN_SLOP_INSTALL_DIR:-$default_dir}"

if [[ "$install_dir" == "$repo_dir" ]]; then
  echo "Design Slop is already available at ${install_dir}"
  exit 0
fi

mkdir -p "$install_dir/references"
install -m 0644 "$repo_dir/SKILL.md" "$install_dir/SKILL.md"
install -m 0644 "$repo_dir/references/slop-patterns.md" "$install_dir/references/slop-patterns.md"
install -m 0644 "$repo_dir/references/before-after-examples.md" "$install_dir/references/before-after-examples.md"

echo "Installed Design Slop at ${install_dir}"
