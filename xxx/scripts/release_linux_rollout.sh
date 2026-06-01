#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)"
TOOL_DIR="${REPO_ROOT}/tools/linux_rollout"
LOCAL_BIN="${HOME}/.local/bin"
NIX_PROFILE_SCRIPT="${LINUX_ROLLOUT_NIX_PROFILE_SCRIPT:-/nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh}"
NIX_BIN_DIR="${LINUX_ROLLOUT_NIX_BIN_DIR:-/nix/var/nix/profiles/default/bin}"

path_prepend() {
  if [ -z "${1:-}" ]; then
    return 0
  fi

  case ":${PATH-}:" in
    *":$1:"*) ;;
    *)
      if [ -n "${PATH-}" ]; then
        PATH="$1:${PATH}"
      else
        PATH="$1"
      fi
      ;;
  esac
}

if ! command -v nix >/dev/null 2>&1; then
  if [ -r "${NIX_PROFILE_SCRIPT}" ]; then
    # shellcheck disable=SC1090
    . "${NIX_PROFILE_SCRIPT}"
  fi

  if ! command -v nix >/dev/null 2>&1 && [ -d "${NIX_BIN_DIR}" ]; then
    path_prepend "${NIX_BIN_DIR}"
  fi
fi

if [ -d "${LOCAL_BIN}" ]; then
  path_prepend "${LOCAL_BIN}"
fi
export PATH

cd "${TOOL_DIR}"

if [ ! -d deps/yaml_elixir ] || [ ! -d deps/ymlr ]; then
  mix deps.get
fi

exec mix run -e 'LinuxRollout.CLI.main(System.argv())' -- "$@" --cwd "${REPO_ROOT}"
