#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${repo_root}"

build_dir="${DANS_UTIL_BUILD_DIR:-build}"
target="dans_util_scratch"

cmake_args=(-S . -B "${build_dir}")

cmake "${cmake_args[@]}"

build_log="$(mktemp "${TMPDIR:-/tmp}/dans_util_build.XXXXXX")"
cleanup() {
    rm -f "${build_log}"
}
trap cleanup EXIT

if ! cmake --build "${build_dir}" --target "${target}" -j >"${build_log}" 2>&1; then
    cat "${build_log}" >&2
    exit 1
fi

exec "${build_dir}/${target}" "$@"
