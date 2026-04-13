#!/bin/bash
# ============================================================
#  Ancient Voices — AU Reset & Logic Pro Relaunch
#  Tully EDM Vibe / Tully Technology Solutions
# ============================================================
#  Quits Logic, kills the AU registrar, revalidates the
#  AncientVoices component, then relaunches Logic Pro.
#  Run as your user — DO NOT sudo.
# ============================================================

set -u

# ----- Plugin identity (from Info.plist of installed bundle) -----
AU_TYPE="aufx"
AU_SUBTYPE="AV06"
AU_MANU="Tull"
PLUGIN_NAME="Ancient Voices"
COMPONENT_PATH="$HOME/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"

# ----- Colors -----
BOLD=$'\033[1m'
DIM=$'\033[2m'
GREEN=$'\033[32m'
YELLOW=$'\033[33m'
RED=$'\033[31m'
CYAN=$'\033[36m'
MAGENTA=$'\033[35m'
RESET=$'\033[0m'

# ----- Helpers -----
banner() {
    echo
    echo "${MAGENTA}============================================================${RESET}"
    echo "${MAGENTA}  $1${RESET}"
    echo "${MAGENTA}============================================================${RESET}"
}

step() {
    echo
    echo "${CYAN}${BOLD}▶ $1${RESET}"
}

ok()    { echo "  ${GREEN}✓${RESET} $1"; }
warn()  { echo "  ${YELLOW}!${RESET} $1"; }
fail()  { echo "  ${RED}✗${RESET} $1"; }
info()  { echo "  ${DIM}· $1${RESET}"; }

spinner() {
    local pid=$1
    local msg=$2
    local frames='⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏'
    local i=0
    while kill -0 "$pid" 2>/dev/null; do
        for f in $frames; do
            printf "\r  ${CYAN}%s${RESET} %s" "$f" "$msg"
            sleep 0.08
            kill -0 "$pid" 2>/dev/null || break
        done
        i=$((i+1))
    done
    printf "\r  ${GREEN}✓${RESET} %s\n" "$msg"
}

# ============================================================
banner "ANCIENT VOICES — AU RESET & LOGIC RELAUNCH"
echo "  ${DIM}Plugin:    ${RESET}${PLUGIN_NAME}"
echo "  ${DIM}AU Codes:  ${RESET}${AU_TYPE} ${AU_SUBTYPE} ${AU_MANU}"
echo "  ${DIM}Bundle:    ${RESET}${COMPONENT_PATH}"

# ----- Sanity check: not running as root -----
if [ "$(id -u)" -eq 0 ]; then
    echo
    fail "This script must NOT be run with sudo."
    fail "AU registration uses your user keychain and caches."
    echo "  Run again as: ${BOLD}./reset_ancientvoices.sh${RESET}"
    exit 1
fi

# ----- Step 1: Verify the component exists -----
step "Checking installed component"
if [ -d "$COMPONENT_PATH" ]; then
    BUILD_DATE=$(stat -f "%Sm" -t "%b %d %Y %H:%M" "$COMPONENT_PATH")
    ok "Found bundle"
    info "Build date: $BUILD_DATE"
else
    fail "Component not found at: $COMPONENT_PATH"
    fail "Build the AU target in Xcode first."
    exit 1
fi

# ----- Step 2: Quit Logic Pro -----
step "Quitting Logic Pro"
if pgrep -x "Logic Pro" >/dev/null; then
    osascript -e 'tell application "Logic Pro" to quit' >/dev/null 2>&1
    info "Sent quit signal, waiting for Logic to close..."
    COUNT=0
    while pgrep -x "Logic Pro" >/dev/null && [ $COUNT -lt 30 ]; do
        sleep 0.5
        COUNT=$((COUNT+1))
    done
    if pgrep -x "Logic Pro" >/dev/null; then
        warn "Logic still running after 15s, force-killing"
        killall -9 "Logic Pro" 2>/dev/null
        sleep 1
    fi
    ok "Logic Pro closed"
else
    info "Logic Pro was not running"
    ok "Skipped"
fi

# ----- Step 3: Kill AU registrar -----
step "Resetting Audio Unit registrar"
killall -9 AudioComponentRegistrar 2>/dev/null && ok "Registrar killed" || info "Registrar was not running"

# ----- Step 4: Clear AU cache -----
step "Clearing AU cache"
CACHE_DIR="$HOME/Library/Caches/AudioUnitCache"
if [ -d "$CACHE_DIR" ]; then
    rm -f "$CACHE_DIR"/com.apple.audiounits.cache 2>/dev/null
    ok "Cache cleared"
else
    info "No cache directory present"
fi

# ----- Step 5: Validate the component -----
step "Validating AncientVoices via auval"
echo "  ${DIM}Running: auval -v ${AU_TYPE} ${AU_SUBTYPE} ${AU_MANU}${RESET}"
echo

# Run auval and capture both output and exit code
AUVAL_LOG=$(mktemp /tmp/auval_av.XXXXXX)
auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANU" > "$AUVAL_LOG" 2>&1 &
AUVAL_PID=$!
spinner "$AUVAL_PID" "Validating component (this takes ~10–30 seconds)"
wait "$AUVAL_PID"
AUVAL_EXIT=$?

if grep -q "AU VALIDATION SUCCEEDED" "$AUVAL_LOG"; then
    ok "Validation succeeded"
    # Pull the headline summary lines for context
    grep -E "VALIDATING|SUCCEEDED|TIME TO OPEN|cold open|warm open" "$AUVAL_LOG" | sed 's/^/    /'
elif [ $AUVAL_EXIT -eq 0 ]; then
    ok "auval returned success (no explicit success line — output below)"
    tail -20 "$AUVAL_LOG" | sed 's/^/    /'
else
    fail "Validation FAILED (exit code $AUVAL_EXIT)"
    echo
    echo "  ${YELLOW}--- Last 30 lines of auval output ---${RESET}"
    tail -30 "$AUVAL_LOG" | sed 's/^/    /'
    echo "  ${YELLOW}--- Full log saved at: $AUVAL_LOG ---${RESET}"
    echo
    fail "Logic Pro will NOT be relaunched. Fix the validation error first."
    exit 1
fi
rm -f "$AUVAL_LOG"

# ----- Step 6: Relaunch Logic Pro -----
step "Relaunching Logic Pro"
open -a "Logic Pro" && ok "Logic Pro launching" || { fail "Could not launch Logic Pro"; exit 1; }

# ----- Done -----
banner "RESET COMPLETE"
echo "  ${GREEN}${BOLD}AncientVoices is revalidated and ready.${RESET}"
echo "  ${DIM}In Logic: insert as Audio FX → Audio Units → Tully EDM Vibe → Ancient Voices${RESET}"
echo
