#!/usr/bin/env bash

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
skill_file="${repo_dir}/SKILL.md"

required_files=(
  "$skill_file"
  "${repo_dir}/references/slop-patterns.md"
  "${repo_dir}/references/before-after-examples.md"
  "${repo_dir}/README.md"
  "${repo_dir}/LICENSE"
)

for file in "${required_files[@]}"; do
  if [[ ! -s "$file" ]]; then
    echo "Missing or empty required file: ${file}" >&2
    exit 1
  fi
done

first_line="$(sed -n '1p' "$skill_file")"
if [[ "$first_line" != "---" ]]; then
  echo "SKILL.md must begin with YAML frontmatter." >&2
  exit 1
fi

for field in "name: design-slop" "description:" "user-invokable: true"; do
  if ! grep -Fq "$field" "$skill_file"; then
    echo "SKILL.md is missing required metadata: ${field}" >&2
    exit 1
  fi
done

for reference in "references/slop-patterns.md" "references/before-after-examples.md"; do
  if ! grep -Fq "$reference" "$skill_file"; then
    echo "SKILL.md does not load ${reference}" >&2
    exit 1
  fi
done

if grep -RInE '(/Users/|~/.env|cookies\.json|gho_|github_pat_)' \
  "$skill_file" "${repo_dir}/references" "${repo_dir}/README.md" "${repo_dir}/CONTRIBUTING.md"; then
  echo "Found a private path or credential-like value." >&2
  exit 1
fi

echo "Design Slop validation passed."
