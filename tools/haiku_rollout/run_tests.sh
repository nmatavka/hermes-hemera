#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "${SCRIPT_DIR}"

if [ ! -d deps/yaml_elixir ] || [ ! -d deps/jason ]; then
  mix deps.get
fi

exec env MIX_ENV=test mix test
