#!/usr/bin/env bash
# Sets up / inspects the macOS pieces the LLDB-based debugger relies on.
#
# For the common case (debugging a locally built binary that the debugger
# launches itself) you do not need any of this: liblldb spawns Apple's signed
# debugserver, which already holds the task_for_pid privilege. This script is
# for the extras: turning on developer mode so attaching to an already-running
# process does not prompt, and signing a binary with the debugger entitlements
# if you ever stop using Apple's debugserver.
#
#   scripts/setup_macos_debugger.sh            # inspect the toolchain
#   scripts/setup_macos_debugger.sh --enable   # enable developer mode (sudo)
#   scripts/setup_macos_debugger.sh --sign PATH # codesign PATH with entitlements
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "this script is macOS-only" >&2
  exit 1
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
entitlements="$here/dans/dbg/debugger.entitlements"

find_debugserver() {
  local candidates=(
    "/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Resources/debugserver"
    "/Library/Developer/CommandLineTools/Library/PrivateFrameworks/LLDB.framework/Resources/debugserver"
  )
  for c in "${candidates[@]}"; do
    if [[ -x "$c" ]]; then echo "$c"; return 0; fi
  done
  return 1
}

find_liblldb() {
  local candidates=(
    "/opt/homebrew/opt/llvm/lib/liblldb.dylib"
    "/usr/local/opt/llvm/lib/liblldb.dylib"
  )
  for c in "${candidates[@]}"; do
    if [[ -f "$c" ]]; then echo "$c"; return 0; fi
  done
  return 1
}

inspect() {
  echo "xcode-select: $(xcode-select -p 2>/dev/null || echo '<none>')"
  echo "lldb:         $(command -v lldb || echo '<none>')  ($(lldb --version 2>/dev/null | head -1))"
  echo "liblldb:      $(find_liblldb || echo '<none>')"
  if ds="$(find_debugserver)"; then
    echo "debugserver:  $ds"
    if codesign -d --entitlements - "$ds" 2>/dev/null | grep -q 'debugger'; then
      echo "              (entitled for debugging: OK)"
    fi
  else
    echo "debugserver:  <none found> -- install Xcode or the Command Line Tools"
  fi
  echo "developer mode: $(DevToolsSecurity -status 2>/dev/null || echo '<unknown>')"
}

case "${1:-}" in
  --enable)
    echo "enabling developer mode (needs sudo)..."
    sudo DevToolsSecurity -enable
    ;;
  --sign)
    target="${2:?usage: --sign PATH}"
    echo "signing $target with $entitlements"
    codesign --force --sign - --entitlements "$entitlements" "$target"
    codesign -d --entitlements - "$target" 2>/dev/null || true
    ;;
  ""|--inspect)
    inspect
    ;;
  *)
    echo "unknown argument: $1" >&2
    exit 2
    ;;
esac
