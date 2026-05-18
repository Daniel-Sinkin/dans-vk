#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${repo_root}"

if ! command -v watchexec >/dev/null 2>&1; then
    echo "watcher.sh requires watchexec. Install it with: brew install watchexec" >&2
    exit 127
fi

exec watchexec \
    --restart \
    --quiet \
    --clear=reset \
    --postpone \
    --stop-signal SIGTERM \
    --stop-timeout 2s \
    --watch app \
    --watch dans/vk \
    --ignore "build/**" \
    --ignore "build-*/**" \
    --ignore "run/**" \
    --ignore "imgui.ini" \
    --ignore "**/*.swp" \
    --ignore "**/*.swo" \
    --ignore "**/*~" \
    --ignore "**/.DS_Store" \
    -- ./run.sh -- "$@"
