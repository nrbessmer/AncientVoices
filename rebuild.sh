#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
#  Ancient Voices — Fast Incremental Rebuild
#
#  Use this every time you change source code.
#  Only recompiles files that changed — typically 2-10 seconds.
#  Requires build.sh to have been run at least once first.
#
#  Usage:
#    ./rebuild.sh              Rebuild + install
#    ./rebuild.sh --debug      Debug build
#    ./rebuild.sh --no-install Compile only, don't copy to plugin folders
#    ./rebuild.sh --auval      Also run auval after install
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

# Request sudo upfront so the system Components copy doesn't prompt mid-build
sudo -v

# ── Fixed project root ─────────────────────────────────────────────────────────
PROJECT="/Volumes/Rocket_XTRM/Projects/AncientVoices"
BUILD_DIR="$PROJECT/build"

# ── Plugin identity ────────────────────────────────────────────────────────────
PLUGIN_NAME="Ancient Voices"
PRODUCT_NAME="AncientVoices"
MANUFACTURER_CODE="Tull"
PLUGIN_CODE="AV05"
AU_TYPE="aufx"    # audio effect

# ── Install destinations ───────────────────────────────────────────────────────
AU_INSTALL="$HOME/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
VST3_INSTALL="$HOME/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"

# ── Colours ───────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

# ── Defaults ──────────────────────────────────────────────────────────────────
CONFIG="Release"
INSTALL=1
RUN_AUVAL=0
JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

for arg in "$@"; do
    case "$arg" in
        --debug)      CONFIG="Debug" ;;
        --no-install) INSTALL=0 ;;
        --auval)      RUN_AUVAL=1 ;;
        -h|--help)
            echo "Usage: $0 [--debug] [--no-install] [--auval]"
            echo "Run build.sh first if you have not built before."
            exit 0 ;;
        *) echo -e "${RED}Unknown option: $arg${RESET}"; exit 1 ;;
    esac
done

ok()   { echo -e "  ${GREEN}✓${RESET}  $*"; }
warn() { echo -e "  ${YELLOW}⚠${RESET}  $*"; }
fail() { echo -e "  ${RED}✗${RESET}  $*"; exit 1; }
sep()  { echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; }

START=$SECONDS
sep
echo -e "\n  ${BOLD}Ancient Voices — Incremental Rebuild${RESET}  [${CONFIG}]"
echo    "  Project: $PROJECT"
sep

# ── Guards ────────────────────────────────────────────────────────────────────

[[ -d "/Volumes/Rocket_XTRM" ]] || fail "Rocket_XTRM drive not mounted"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo -e "  ${RED}Build not configured.${RESET}"
    echo "  Run this first: $PROJECT/build.sh"
    exit 1
fi

# ── Compile ───────────────────────────────────────────────────────────────────

echo "  Compiling changed files..."

cmake --build "$BUILD_DIR" \
    --config "$CONFIG" \
    --parallel "$JOBS" 2>&1 | tee /tmp/av_rebuild.log | \
    grep -E "(Compiling|Linking|error:|warning:|up-to-date)" | \
    grep -v "^$" | \
    while IFS= read -r line; do
        if echo "$line" | grep -q "error:"; then
            echo -e "  ${RED}$line${RESET}"
        elif echo "$line" | grep -q "warning:"; then
            echo -e "  ${YELLOW}$line${RESET}"
        elif echo "$line" | grep -q "up-to-date"; then
            echo -e "  ${CYAN}$line${RESET}"
        else
            echo "  $line"
        fi
    done

BUILD_STATUS=${PIPESTATUS[0]}

if [[ $BUILD_STATUS -ne 0 ]]; then
    echo ""
    echo -e "${RED}  Compile FAILED. Last 40 lines:${RESET}"
    tail -40 /tmp/av_rebuild.log | sed 's/^/  /'
    echo ""
    echo "  Full log: /tmp/av_rebuild.log"
    exit 1
fi

COMPILE_ELAPSED=$((SECONDS - START))
ok "Compiled in ${COMPILE_ELAPSED}s"

# ── Install ───────────────────────────────────────────────────────────────────

if [[ $INSTALL -eq 1 ]]; then
    AU_BUILD="$BUILD_DIR/${PRODUCT_NAME}_artefacts/${CONFIG}/AU/${PLUGIN_NAME}.component"
    VST3_BUILD="$BUILD_DIR/${PRODUCT_NAME}_artefacts/${CONFIG}/VST3/${PLUGIN_NAME}.vst3"

    mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"

    if [[ -d "$AU_BUILD" ]]; then
        rm -rf "$AU_INSTALL"
        cp -r "$AU_BUILD" "$AU_INSTALL"
        ok "AU   → $AU_INSTALL"
        sudo rm -rf "/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
        sudo cp -r "$AU_BUILD" "/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
        ok "AU   → /Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
    else
        warn "AU bundle not found — did the compile succeed?"
    fi

    if [[ -d "$VST3_BUILD" ]]; then
        rm -rf "$VST3_INSTALL"
        cp -r "$VST3_BUILD" "$VST3_INSTALL"
        ok "VST3 → $VST3_INSTALL"
    fi

    # Clear AU component cache so Logic sees the update immediately
    killall -q AudioComponentRegistrar 2>/dev/null || true
    ok "AU component cache cleared"
fi

# ── Optional auval ────────────────────────────────────────────────────────────

if [[ $RUN_AUVAL -eq 1 ]]; then
    sleep 1
    echo "  Running auval..."
    AUVAL_OUT=$(auval -v "${AU_TYPE}" "${PLUGIN_CODE}" "${MANUFACTURER_CODE}" 2>&1)
    if echo "$AUVAL_OUT" | grep -q "\* \* PASS \* \*"; then
        ok "auval PASS"
    else
        warn "auval issues — run full build.sh for details"
        echo "$AUVAL_OUT" | grep -E "(FAIL|error)" | sed 's/^/    /' || true
    fi
fi

# ── Done ──────────────────────────────────────────────────────────────────────

TOTAL=$((SECONDS - START))
sep
echo ""
echo -e "  ${GREEN}${BOLD}Done${RESET}  (${TOTAL}s)"
echo ""
echo "  To reload in Logic:"
echo "    Close and reopen your project, OR"
echo "    Preferences → Plug-in Manager → Reset & Rescan"
echo ""
echo "  Commands:"
echo "    Full rebuild:  $PROJECT/build.sh --clean"
echo "    With auval:    $PROJECT/rebuild.sh --auval"
sep
echo ""
