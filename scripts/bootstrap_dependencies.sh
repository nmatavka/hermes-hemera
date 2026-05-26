#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY_DIR="third_party"

mkdir -p "${ROOT_DIR}/${THIRD_PARTY_DIR}"

ensure_checkout() {
    local name="$1"
    local url="$2"
    local relative_path="$3"
    local checkout_ref="$4"
    local tracked_branch="${5:-}"
    local absolute_path="${ROOT_DIR}/${relative_path}"

    if git -C "${ROOT_DIR}" rev-parse --git-dir >/dev/null 2>&1; then
        if [ ! -d "${absolute_path}/.git" ] && [ ! -f "${absolute_path}/.git" ]; then
            git -C "${ROOT_DIR}" submodule add --force "${url}" "${relative_path}"
        fi

        if [ -n "${tracked_branch}" ]; then
            git -C "${ROOT_DIR}" config -f .gitmodules submodule."${relative_path}".branch "${tracked_branch}"
        fi

        git -C "${absolute_path}" fetch --tags origin
        git -C "${absolute_path}" checkout "${checkout_ref}"
        return
    fi

    if [ -d "${absolute_path}/.git" ]; then
        git -C "${absolute_path}" fetch --tags origin
        git -C "${absolute_path}" checkout "${checkout_ref}"
    else
        git clone --depth 1 "${url}" "${absolute_path}"
        git -C "${absolute_path}" fetch --tags origin
        git -C "${absolute_path}" checkout "${checkout_ref}"
    fi
}

ensure_checkout "Hermes-Paige" "https://github.com/nmatavka/Hermes-Paige" "${THIRD_PARTY_DIR}/Hermes-Paige" "main" "main"
ensure_checkout "openssl" "https://github.com/openssl/openssl" "${THIRD_PARTY_DIR}/openssl" "openssl-4.0.0"
ensure_checkout "hunspell" "https://github.com/hunspell/hunspell" "${THIRD_PARTY_DIR}/hunspell" "v1.7.3"
