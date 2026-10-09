#!/usr/bin/env bash
#
# Runs testflow test cases against a local build:
#
#   run_testflow.sh --all
#   run_testflow.sh TC1.1_BasicTest
#   run_testflow.sh TC3.1_CrashRecovery
#   run_testflow.sh --step-by-step TC3.1_CrashRecovery
#
# A test case is a script or a series.
# A series is a .testflow file listing scripts, one per line, relative
# to it; lines starting with # are comments. Its scripts run in that order, as
# successive runs of the app sharing one profile, and stop at the first failure.
#
# --step-by-step starts each run of the app paused, to step through it from this
# terminal: Enter plays the next step, a + Enter all remaining ones, p + Enter pauses.
# The app's output then goes to a log file, of which only the outcome is shown.
# On Linux, the app then shows in a window of its own, a nested X server (Xephyr) with
# its own window manager (openbox), so that it does not take focus from this terminal
# at each run.
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

step_by_step=
if [ "${1:-}" = "--step-by-step" ]; then
    step_by_step=--test-case-step-by-step
    shift
fi

if [ "${1:-}" = "--all" ]; then
    set --
    for path in "$SCRIPTS_DIR"/*.js "$SCRIPTS_DIR"/*.testflow; do
        name=${path#"$SCRIPTS_DIR"/}
        grep -q "\"$name\"" "$SCRIPTS_DIR/disabled.json" || set -- "$@" "$name"
    done
fi

test $# -gt 0 || { echo "usage: $(basename "$0") [--step-by-step] [--all | <test case>...]"; exit 1; }

export MUSE_TESTFLOW_SCRIPTS_PATH="$SCRIPTS_DIR"
export MUSE_TESTFLOW_DATA_PATH="$PWD/$BUILD_DIR/testflow_data"
export AU_ALLOW_MULTIPLE_PROCESSES=1
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:new_delete_type_mismatch=0}

run_script() {
    if [ -n "$step_by_step" ]; then
        local log
        log="$MUSE_TESTFLOW_DATA_PATH/logs/$(basename "$1" .js).log"
        mkdir -p "$(dirname "$log")"
        # Slow enough to follow when playing all. The steps are announced straight to the terminal
        local rc=0
        "$APP" --test-case "$1" --test-case-speed Fast "$step_by_step" > "$log" 2>&1 || rc=$?
        local result=PASSED
        test $rc -eq 0 || result=FAILED
        # Once is enough: the app prints them again when given anything it does not know
        export AU_TESTFLOW_STEPPER_NO_INSTRUCTIONS=1
        echo "$result, log: ${log#"$PWD"/}"
        # Without the log's colour codes
        grep -a -o 'failed exec step: .*' "$log" | sed 's/\x1b\[[0-9;]*m//g' || true
        return $rc
    else
        "$APP" --test-case "$1" --test-case-speed Fast
    fi
}

# The file of a test case given by path or by name, or nothing
resolve() {
    local candidate
    for candidate in "$1" "$1.testflow" "$1.js" "$SCRIPTS_DIR/$1" "$SCRIPTS_DIR/$1.testflow" "$SCRIPTS_DIR/$1.js"; do
        # Files only: a series' directory is not a test case
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return
        fi
    done
}

run_series() {
    local file=$1
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

# Resolved up front
files=()
for name in "$@"; do
    file=$(resolve "$name")
    test -n "$file" || { echo "no such test case: $name"; exit 1; }
    files+=("$file")
done

# Each run opens new windows, which testflow also activates: inside a nested X server,
# they do not take focus from the terminal stepping through them
start_nested_display() {
    if ! command -v Xephyr > /dev/null; then
        echo "(Xephyr not found, so Audacity takes focus at each run: install it to avoid that, e.g. with sudo apt install xserver-xephyr)"
        return
    fi

    local fifo
    fifo=$(mktemp -u)
    mkfifo "$fifo"
    # Xephyr writes the display it picked once ready to connect
    Xephyr -displayfd 3 -screen "${TESTFLOW_SCREEN:-1600x1000}" -resizeable -no-host-grab -title "Audacity testflow" \
        3> "$fifo" > /dev/null 2>&1 &
    nested_display_pid=$!
    nested_wm_pid=
    nested_resize_pid=
    nested_wm_config=$(mktemp)
    trap 'kill $nested_resize_pid $nested_wm_pid "$nested_display_pid" 2> /dev/null || true; rm -f "$nested_wm_config"' EXIT

    local number=
    read -r number < "$fifo" || true
    rm -f "$fifo"
    if [ -z "$number" ]; then
        echo "(Xephyr did not start, so Audacity takes focus at each run)"
        return
    fi
    export DISPLAY=":$number"

    # Gives the windows inside, dialogs included, borders and title bars
    if command -v openbox > /dev/null; then
        # Openbox's defaults, plus main windows maximized, so they follow the size of Xephyr's window
        sed '/<applications>/a <application class="*" type="normal"><maximized>yes</maximized></application>' \
            /etc/xdg/openbox/rc.xml > "$nested_wm_config" 2> /dev/null || true
        if [ -s "$nested_wm_config" ]; then
            openbox --sm-disable --config-file "$nested_wm_config" > /dev/null 2>&1 &
        else
            openbox --sm-disable > /dev/null 2>&1 &
        fi
        nested_wm_pid=$!

        if command -v xdpyinfo > /dev/null && command -v xrandr > /dev/null; then
            replay_nested_resizes &
            nested_resize_pid=$!
        fi
    else
        echo "(openbox not found, so windows have no borders: install it to have them, e.g. with sudo apt install openbox)"
    fi
}

# Xephyr resizes its screen with its window, but leaves the mode of its output as it was,
# which is where openbox and Qt read the screen size from. Switches the output to a mode
# of the new size, which they then follow: Qt's screen is resized rather than replaced.
# Ends with Xephyr
replay_nested_resizes() {
    local last= size output
    while sleep 1; do
        size=$(xdpyinfo -display "$DISPLAY" 2> /dev/null | awk '/dimensions:/ { print $2 }')
        [ -n "$size" ] || break
        if [ -n "$last" ] && [ "$size" != "$last" ]; then
            local w=${size%x*} h=${size#*x}
            output=$(xrandr -display "$DISPLAY" 2> /dev/null | awk '/ connected/ { print $1; exit }')
            # The mode may exist from an earlier resize to that size
            xrandr -display "$DISPLAY" --newmode "$size" 0 "$w" "$w" "$w" "$w" "$h" "$h" "$h" "$h" > /dev/null 2>&1 || true
            xrandr -display "$DISPLAY" --addmode "$output" "$size" > /dev/null 2>&1 || true
            xrandr -display "$DISPLAY" --output "$output" --mode "$size" > /dev/null 2>&1 || true
        fi
        last=$size
    done
}

if [ -n "$step_by_step" ] && [ "$(uname)" = Linux ] && [ "${AU_QT_QPA_PLATFORM:-}" != offscreen ]; then
    start_nested_display
fi

failed=
for i in "${!files[@]}"; do
    case "${files[$i]}" in
        *.testflow) run_series "${files[$i]}" || failed="$failed ${*:i+1:1}" ;;
        *)          run_script "${files[$i]}" || failed="$failed ${*:i+1:1}" ;;
    esac
done

test -z "$failed" || { echo "failed:$failed"; exit 1; }
