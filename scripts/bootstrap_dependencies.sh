#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY_DIR="third_party"
DEPENDENCY_REFS_FILE="${ROOT_DIR}/cmake/HermesDependencyRefs.env"
PAIGE_PATCH_DIR="${ROOT_DIR}/cmake/patches/hermes-paige"

if [ ! -f "${DEPENDENCY_REFS_FILE}" ]; then
    echo "missing dependency ref manifest: ${DEPENDENCY_REFS_FILE}" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "${DEPENDENCY_REFS_FILE}"

mkdir -p "${ROOT_DIR}/${THIRD_PARTY_DIR}"

apply_patchset() {
    local absolute_path="$1"
    local patch_dir="$2"

    if [ ! -d "${patch_dir}" ]; then
        return
    fi

    local patch_path
    for patch_path in "${patch_dir}"/*.patch; do
        if [ ! -f "${patch_path}" ]; then
            continue
        fi

        if git -C "${absolute_path}" apply --reverse --check "${patch_path}" >/dev/null 2>&1; then
            continue
        fi

        git -C "${absolute_path}" apply "${patch_path}"
    done
}

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
        if [ "${name}" = "Hermes-Paige" ]; then
            apply_patchset "${absolute_path}" "${PAIGE_PATCH_DIR}"
        fi
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

    if [ "${name}" = "Hermes-Paige" ]; then
        apply_patchset "${absolute_path}" "${PAIGE_PATCH_DIR}"
    fi
}

ensure_checkout "Hermes-Paige" "${HEMERA_DEP_HERMES_PAIGE_REPOSITORY}" "${THIRD_PARTY_DIR}/Hermes-Paige" "${HEMERA_DEP_HERMES_PAIGE_REF}"
ensure_checkout "openssl" "${HEMERA_DEP_OPENSSL_REPOSITORY}" "${THIRD_PARTY_DIR}/openssl" "${HEMERA_DEP_OPENSSL_REF}"
ensure_checkout "hunspell" "${HEMERA_DEP_HUNSPELL_REPOSITORY}" "${THIRD_PARTY_DIR}/hunspell" "${HEMERA_DEP_HUNSPELL_REF}"
ensure_checkout "krb5" "${HEMERA_DEP_KRB5_REPOSITORY}" "${THIRD_PARTY_DIR}/krb5" "${HEMERA_DEP_KRB5_REF}"
