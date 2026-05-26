#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:?repository root required}"
forbidden="legacy/eudora-drop/"

declare -a targets=(
  "$repo_root/CMakeLists.txt"
  "$repo_root/tests/CMakeLists.txt"
)

while IFS= read -r file; do
  targets+=("$file")
done < <(find "$repo_root/src" "$repo_root/cmake" -type f | sort)

if command -v rg >/dev/null 2>&1; then
  if rg -n --fixed-strings "$forbidden" "${targets[@]}"; then
    echo "Forbidden archive path reference detected in build-driving files."
    exit 1
  fi
else
  if grep -R -n --fixed-strings "$forbidden" "${targets[@]}"; then
    echo "Forbidden archive path reference detected in build-driving files."
    exit 1
  fi
fi
