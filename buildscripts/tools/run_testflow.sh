#!/usr/bin/env bash
#
# Runs testflow test cases against a local build:
#
#   run_testflow.sh --all
#   run_testflow.sh TC1.1_BasicTest.js
#   run_testflow.sh TC3.1_CrashRecovery.testflow
#
# A test case is a script or a series, given by name in share/testflowscripts or
# by path. A series is a .testflow file listing scripts, one per line, relative
# to it; lines starting with # are comments. Its scripts run in that order, as
# successive runs of the app sharing one profile, and stop at the first failure.
#
# Set BUILD_DIR for a build other than the default. Single scripts run against
# your own settings. Series run in a throwaway profile, which only works on Linux.

set -euo pipefail

cd "$(dirname "$0")/../.."

BUILD_DIR=${BUILD_DIR:-build/audacity-release}
SCRIPTS_DIR="$PWD/share/testflowscripts"

case "$(uname)" in
    Darwin) APP="$BUILD_DIR/src/app/audacity.app/Contents/MacOS/audacity" ;;
    *)      APP="$BUILD_DIR/src/app/audacity" ;;
esac
test -x "$APP" || { echo "no build at $APP, set BUILD_DIR"; exit 1; }

if [ "${1:-}" = "--all" ]; then
    set --
    for path in "$SCRIPTS_DIR"/*.js "$SCRIPTS_DIR"/*.testflow; do
        name=${path#"$SCRIPTS_DIR"/}
        grep -q "\"$name\"" "$SCRIPTS_DIR/disabled.json" || set -- "$@" "$name"
    done
fi

test $# -gt 0 || { echo "usage: $(basename "$0") [--all | <test case>...]"; exit 1; }

export MUSE_TESTFLOW_SCRIPTS_PATH="$SCRIPTS_DIR"
export MUSE_TESTFLOW_DATA_PATH="$PWD/$BUILD_DIR/testflow_data"
export AU_ALLOW_MULTIPLE_PROCESSES=1
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:new_delete_type_mismatch=0}

run_script() {
    "$APP" --test-case "$1" --test-case-speed Fast
}

run_series() {
    local file=$1
    test -e "$file" || file="$SCRIPTS_DIR/$file"
    local dir
    dir=$(dirname "$file")

    # A subshell, so the sandbox does not leak into the next test case
    (
        if [ "$(uname)" = Darwin ]; then
            echo "warning: $1 runs against your own profile and may leave projects to recover in it"
        else
            sandbox=$(mktemp -d)
            trap 'rm -rf "$sandbox"' EXIT
            export XDG_CONFIG_HOME="$sandbox/config" XDG_DATA_HOME="$sandbox/data" \
                   XDG_CACHE_HOME="$sandbox/cache" XDG_STATE_HOME="$sandbox/state"
        fi

        # On fd 3, so the app cannot consume the list from stdin
        while read -r script <&3 || [ -n "$script" ]; do
            case "$script" in ''|'#'*) continue ;; esac
            test -e "$dir/$script" || { echo "$file lists $script, which does not exist"; return 1; }
            run_script "$dir/$script" || return 1
        done 3< "$file"
    )
}

# Checked up front, so that e.g. a cancelled IDE file picker starts nothing
for name in "$@"; do
    test -n "$name" && { test -e "$name" || test -e "$SCRIPTS_DIR/$name" || test -e "$SCRIPTS_DIR/$name.js"; } \
        || { echo "no such test case: $name"; exit 1; }
done

failed=
for name in "$@"; do
    case "$name" in
        *.testflow) run_series "$name" || failed="$failed $name" ;;
        *)          run_script "$name" || failed="$failed $name" ;;
    esac
done

test -z "$failed" || { echo "failed:$failed"; exit 1; }
