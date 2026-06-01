#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)"
TOOL_DIR="${REPO_ROOT}/tools/haiku_rollout"
LOCAL_BIN="${HOME}/.local/bin"

if [ -d "${LOCAL_BIN}" ]; then
  PATH="${LOCAL_BIN}:${PATH}"
fi
export PATH

cd "${TOOL_DIR}"

if [ ! -d deps/yaml_elixir ] || [ ! -d deps/jason ]; then
  mix deps.get
fi

exec mix run -e 'HemeraHaikuRollout.CLI.main(System.argv())' -- "$@" --cwd "${REPO_ROOT}"
