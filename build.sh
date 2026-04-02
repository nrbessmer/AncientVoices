#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
#  Ancient Voices — Full Build & Deploy
#
#  Phases:
#    1 — Validate environment
#    2 — Check JUCE submodule
#    3 — CMake configure (if needed)
#    4 — Compile
#    5 — Install AU + VST3 → ~/Library
#    6 — auval validation
#
#  Usage:
#    ./build.sh              Normal build
#    ./build.sh --clean      Wipe build/ and rebuild from scratch
#    ./build.sh --universal  arm64 + x86_64 universal binary
#    ./build.sh --debug      Debug configuration
#    ./build.sh --no-auval   Skip AU validation (faster)
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

# ── Fixed project root ─────────────────────────────────────────────────────────
PROJECT="/Volumes/Rocket_XTRM/Projects/AncientVoices"
BUILD_DIR="$PROJECT/build"
JUCE_DIR="$PROJECT/JUCE"

# ── Plugin identity (must match CMakeLists.txt exactly) ───────────────────────
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
CLEAN=0
UNIVERSAL=0
RUN_AUVAL=1
JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

for arg in "$@"; do
    case "$arg" in
        --clean)     CLEAN=1 ;;
        --debug)     CONFIG="Debug" ;;
        --universal) UNIVERSAL=1 ;;
        --no-auval)  RUN_AUVAL=0 ;;
        -h|--help)
            echo "Usage: $0 [--clean] [--debug] [--universal] [--no-auval]"
            exit 0 ;;
        *) echo -e "${RED}Unknown option: $arg${RESET}"; exit 1 ;;
    esac
done

phase() { echo -e "\n${CYAN}${BOLD}  Phase $1 — $2${RESET}"; }
ok()    { echo -e "  ${GREEN}✓${RESET}  $*"; }
warn()  { echo -e "  ${YELLOW}⚠${RESET}  $*"; }
fail()  { echo -e "  ${RED}✗${RESET}  $*"; exit 1; }
sep()   { echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; }

START=$SECONDS
sep
echo -e "\n  ${BOLD}Ancient Voices — Build${RESET}  [${CONFIG}]"
echo    "  Project: $PROJECT"
echo    "  Codes: Manufacturer ${MANUFACTURER_CODE}  Plugin ${PLUGIN_CODE}  Type ${AU_TYPE}"
sep

# ═══════════════════════════════════════════════════════════════════════════════
phase 1 "Validate environment"
# ═══════════════════════════════════════════════════════════════════════════════

[[ "$(uname)" == "Darwin" ]] || fail "macOS only"

# Drive mounted?
[[ -d "/Volumes/Rocket_XTRM" ]] || fail "Rocket_XTRM drive not mounted"
ok "Rocket_XTRM mounted"

# Source files present?
[[ -f "$PROJECT/Source/PluginProcessor.cpp" ]] || \
    fail "Source files missing from $PROJECT/Source — did you run setup.sh?"
ok "Source files present"

# cmake
command -v cmake &>/dev/null || fail "cmake not found — brew install cmake"
CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
ok "cmake $CMAKE_VER"

# Xcode CLT
xcode-select -p &>/dev/null || fail "Xcode CLT missing — xcode-select --install"
ok "Xcode CLT present"

# xcodebuild
if command -v xcodebuild &>/dev/null; then
    USE_XCODE=1
    ok "Xcode: $(xcodebuild -version 2>/dev/null | head -1)"
else
    USE_XCODE=0
    warn "xcodebuild not found — using Makefile generator"
fi

# auval
if [[ $RUN_AUVAL -eq 1 ]]; then
    if command -v auval &>/dev/null; then
        ok "auval found"
    else
        warn "auval not found — skipping AU validation"
        RUN_AUVAL=0
    fi
fi

# ═══════════════════════════════════════════════════════════════════════════════
phase 2 "Check JUCE submodule"
# ═══════════════════════════════════════════════════════════════════════════════

if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    echo "  JUCE not found — running git submodule update..."
    git -C "$PROJECT" submodule update --init --recursive || \
        fail "Could not fetch JUCE. Check internet connection and run setup.sh."
fi
JUCE_VER=$(cat "$JUCE_DIR/VERSION" 2>/dev/null || echo "unknown")
ok "JUCE $JUCE_VER"

# ═══════════════════════════════════════════════════════════════════════════════
phase 3 "CMake configure"
# ═══════════════════════════════════════════════════════════════════════════════

if [[ $CLEAN -eq 1 ]]; then
    echo "  --clean: removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if [[ $USE_XCODE -eq 1 ]]; then
    GENERATOR="Xcode"
    ARCH_FLAGS=""
    if [[ $UNIVERSAL -eq 1 ]]; then
        ARCH_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
        echo "  Building universal binary (arm64 + x86_64)"
    fi
else
    GENERATOR="Unix Makefiles"
    ARCH_FLAGS="-DCMAKE_BUILD_TYPE=${CONFIG}"
fi

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "  Configuring..."
    cmake -B "$BUILD_DIR" \
        -G "$GENERATOR" \
        $ARCH_FLAGS \
        -DJUCE_BUILD_EXTRAS=OFF \
        -DJUCE_BUILD_EXAMPLES=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        "$PROJECT" 2>&1 | grep -E "^(--|Configuring|Build files)" | head -20 || true
    ok "CMake configured → $BUILD_DIR"
else
    ok "Using existing CMake cache (--clean to force reconfigure)"
fi

# ═══════════════════════════════════════════════════════════════════════════════
phase 4 "Compile  ($CONFIG, $JOBS threads)"
# ═══════════════════════════════════════════════════════════════════════════════

BUILD_START=$SECONDS

cmake --build "$BUILD_DIR" \
    --config "$CONFIG" \
    --parallel "$JOBS" \
    --verbose 2>&1 | tee /tmp/av_build.log | \
    while IFS= read -r line; do
        if echo "$line" | grep -q "error:"; then
            echo -e "  ${RED}$line${RESET}"
        elif echo "$line" | grep -q "warning:"; then
            echo -e "  ${YELLOW}$line${RESET}"
        elif echo "$line" | grep -qE "^(\[|Building|Linking|Compiling|Scanning|cd |clang|g\+\+|ar )"; then
            echo "  $line"
        else
            echo "  $line"
        fi
    done

BUILD_STATUS=${PIPESTATUS[0]}
BUILD_ELAPSED=$((SECONDS - BUILD_START))

if [[ $BUILD_STATUS -ne 0 ]]; then
    echo ""
    echo -e "${RED}  Build FAILED. Last 30 lines:${RESET}"
    tail -30 /tmp/av_build.log | sed 's/^/  /'
    echo ""
    echo "  Full log: /tmp/av_build.log"
    exit 1
fi

ok "Compiled in ${BUILD_ELAPSED}s"

# ═══════════════════════════════════════════════════════════════════════════════
phase 5 "Install AU + VST3"
# ═══════════════════════════════════════════════════════════════════════════════

AU_BUILD="$BUILD_DIR/${PRODUCT_NAME}_artefacts/${CONFIG}/AU/${PLUGIN_NAME}.component"
VST3_BUILD="$BUILD_DIR/${PRODUCT_NAME}_artefacts/${CONFIG}/VST3/${PLUGIN_NAME}.vst3"
APP_BUILD="$BUILD_DIR/${PRODUCT_NAME}_artefacts/${CONFIG}/Standalone/${PLUGIN_NAME}.app"

# Create install directories if missing
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"

if [[ -d "$AU_BUILD" ]]; then
    rm -rf "$AU_INSTALL"
    cp -r "$AU_BUILD" "$AU_INSTALL"
    ok "AU   → $AU_INSTALL"
    # Also copy to system Components so Logic finds it
    sudo rm -rf "/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
    sudo cp -r "$AU_BUILD" "/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
    ok "AU   → /Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
else
    warn "AU bundle not found at $AU_BUILD"
fi

if [[ -d "$VST3_BUILD" ]]; then
    rm -rf "$VST3_INSTALL"
    cp -r "$VST3_BUILD" "$VST3_INSTALL"
    ok "VST3 → $VST3_INSTALL"
else
    warn "VST3 bundle not found at $VST3_BUILD"
fi

[[ -d "$APP_BUILD" ]] && ok "App  → $APP_BUILD"

# ── auval ─────────────────────────────────────────────────────────────────────
if [[ $RUN_AUVAL -eq 1 ]] && [[ -d "$AU_INSTALL" ]]; then
    echo ""
    echo "  Running auval..."

    # Force macOS to re-register the newly installed component
    killall -q AudioComponentRegistrar 2>/dev/null || true
    killall -q coreaudiod 2>/dev/null || true
    sleep 2

    # Run auval — print ALL output so you can see what's happening
    echo ""
    echo "  ── auval output ─────────────────────────────────────────────"
    auval -v "${AU_TYPE}" "${PLUGIN_CODE}" "${MANUFACTURER_CODE}" 2>&1 | tee /tmp/av_auval.log | sed 's/^/  /'
    AUVAL_STATUS=${PIPESTATUS[0]}
    echo "  ─────────────────────────────────────────────────────────────"
    echo ""

    if grep -q "\* \* PASS \* \*" /tmp/av_auval.log; then
        ok "auval  PASS — plugin is valid for Logic Pro"
    else
        warn "auval did not PASS — check output above"
        echo "  Full log saved to: /tmp/av_auval.log"
    fi
fi

# ── Summary ───────────────────────────────────────────────────────────────────
TOTAL=$((SECONDS - START))
sep
echo ""
echo -e "  ${GREEN}${BOLD}Build complete${RESET}  (${TOTAL}s)"
echo ""
[[ -d "$AU_INSTALL"   ]] && echo -e "  ${GREEN}✓${RESET}  AU   $AU_INSTALL"
[[ -d "$VST3_INSTALL" ]] && echo -e "  ${GREEN}✓${RESET}  VST3 $VST3_INSTALL"
[[ -d "$APP_BUILD"    ]] && echo -e "  ${GREEN}✓${RESET}  App  $APP_BUILD"
echo ""
echo "  In Logic: Preferences → Plug-in Manager → Reset & Rescan"
echo "  Look for: Tully EDM Vibe → Ancient Voices"
echo ""
echo "  Commands:"
echo "    Full rebuild:    $PROJECT/build.sh --clean"
echo "    Quick rebuild:   $PROJECT/rebuild.sh"
echo "    Open in Xcode:   open $BUILD_DIR/AncientVoices.xcodeproj"
echo "    Run standalone:  open \"$APP_BUILD\""
echo ""
echo "  Codes: Manufacturer ${MANUFACTURER_CODE}  Plugin ${PLUGIN_CODE}  Type ${AU_TYPE}"
sep
echo ""
