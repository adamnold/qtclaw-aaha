#!/usr/bin/env bash
set -euo pipefail
qtclaw_test_root=$(mktemp -d /tmp/qtclaw-test-env.XXXXXXXX)
trap 'rm -rf -- "$qtclaw_test_root"' EXIT
mkdir -m 700 "$qtclaw_test_root"/{runtime,config,data,cache}
export XDG_RUNTIME_DIR="$qtclaw_test_root/runtime"
export XDG_CONFIG_HOME="$qtclaw_test_root/config"
export XDG_DATA_HOME="$qtclaw_test_root/data"
export XDG_CACHE_HOME="$qtclaw_test_root/cache"
export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu
# No HOME changes, no real desktop notification bus, no browser sandbox bypass.
dbus-run-session -- "$@"
