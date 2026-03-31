#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
#  Ancient Voices — First-Time Setup
#
#  Download ALL files from Claude to ~/Downloads first, then run this script.
#  It does everything: creates directories, copies files, git, JUCE, cmake.
#
#  Usage (two commands, that's it):
#    chmod +x ~/Downloads/setup.sh
#    ~/Downloads/setup.sh
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

DOWNLOADS="$HOME/Downloads"
PROJECT="/Volumes/Rocket_XTRM/Projects/AncientVoices"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

ok()   { echo -e "  ${GREEN}✓${RESET}  $*"; }
warn() { echo -e "  ${YELLOW}⚠${RESET}  $*"; }
fail() { echo -e "\n  ${RED}✗  FAILED:${RESET} $*\n"; exit 1; }
step() { echo -e "\n${CYAN}${BOLD}  ▶  $*${RESET}"; }
sep()  { echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; }

sep
echo -e "\n  ${BOLD}Ancient Voices — First-Time Setup${RESET}"
echo    "  Downloads: $DOWNLOADS"
echo    "  Project:   $PROJECT"
sep

# ═══════════════════════════════════════════════════════════════════════════════
step "Checking prerequisites"
# ═══════════════════════════════════════════════════════════════════════════════

[[ "$(uname)" == "Darwin" ]] || fail "macOS only"

[[ -d "/Volumes/Rocket_XTRM" ]] || \
    fail "Rocket_XTRM not mounted — plug in the drive and try again"
ok "Rocket_XTRM mounted"

command -v cmake &>/dev/null || \
    fail "cmake not found\n  Fix: brew install cmake"
CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
CMAJOR=$(echo "$CMAKE_VER" | cut -d. -f1)
CMINOR=$(echo "$CMAKE_VER" | cut -d. -f2)
if [[ $CMAJOR -lt 3 ]] || { [[ $CMAJOR -eq 3 ]] && [[ $CMINOR -lt 22 ]]; }; then
    fail "cmake $CMAKE_VER is too old (need 3.22+)\n  Fix: brew upgrade cmake"
fi
ok "cmake $CMAKE_VER"

xcode-select -p &>/dev/null || \
    fail "Xcode CLT missing\n  Fix: xcode-select --install"
ok "Xcode CLT: $(xcode-select -p)"

command -v git &>/dev/null || \
    fail "git not found\n  Fix: xcode-select --install"
ok "git $(git --version | awk '{print $3}')"

# ═══════════════════════════════════════════════════════════════════════════════
step "Checking all source files are in $DOWNLOADS"
# ═══════════════════════════════════════════════════════════════════════════════

# Every file that must be in Downloads before this script can run
REQUIRED_FILES=(
    "build.sh"
    "rebuild.sh"
    "CMakeLists.txt"
    "PluginProcessor.h"
    "PluginProcessor.cpp"
    "PluginEditor.h"
    "PluginEditor.cpp"
    "GlottalSource.h"
    "FormantFilter.h"
    "ChoirVoice.h"
    "ChoirEngine.h"
    "HumanizeEngine.h"
    "CaveReverb.h"
    "FormantShimmer.h"
    "VowelTables.h"
    "IntervalTables.h"
    "Presets.h"
    "UI.h"
    "FormantScope.h"
    "VoicePanel.h"
    "AncientPanel.h"
    "FXPanel.h"
)

MISSING=0
for f in "${REQUIRED_FILES[@]}"; do
    if [[ ! -f "$DOWNLOADS/$f" ]]; then
        echo -e "  ${RED}missing:${RESET} $DOWNLOADS/$f"
        MISSING=1
    fi
done

if [[ $MISSING -eq 1 ]]; then
    echo ""
    fail "Some files are missing from $DOWNLOADS\n  Download all files from Claude then run this script again."
fi

ok "All ${#REQUIRED_FILES[@]} required files found in $DOWNLOADS"

# ═══════════════════════════════════════════════════════════════════════════════
step "Removing any previous incomplete setup"
# ═══════════════════════════════════════════════════════════════════════════════

# Clean up the lowercase folder if it exists from a previous attempt
if [[ -d "/Volumes/Rocket_XTRM/Projects/ancientvoices" ]]; then
    rm -rf "/Volumes/Rocket_XTRM/Projects/ancientvoices"
    ok "Removed leftover lowercase ancientvoices folder"
fi

# Remove partial AncientVoices if it exists but has no Source files
if [[ -d "$PROJECT" ]] && [[ ! -f "$PROJECT/Source/PluginProcessor.cpp" ]]; then
    rm -rf "$PROJECT"
    ok "Removed incomplete $PROJECT"
fi

# ═══════════════════════════════════════════════════════════════════════════════
step "Creating project directories"
# ═══════════════════════════════════════════════════════════════════════════════

mkdir -p "$PROJECT/Source/DSP"
mkdir -p "$PROJECT/Source/Data"
mkdir -p "$PROJECT/Source/UI"

ok "$PROJECT/"
ok "$PROJECT/Source/"
ok "$PROJECT/Source/DSP/"
ok "$PROJECT/Source/Data/"
ok "$PROJECT/Source/UI/"

# ═══════════════════════════════════════════════════════════════════════════════
step "Copying files from $DOWNLOADS"
# ═══════════════════════════════════════════════════════════════════════════════

cp "$DOWNLOADS/setup.sh"           "$PROJECT/setup.sh"
cp "$DOWNLOADS/build.sh"           "$PROJECT/build.sh"
cp "$DOWNLOADS/rebuild.sh"         "$PROJECT/rebuild.sh"
cp "$DOWNLOADS/CMakeLists.txt"     "$PROJECT/CMakeLists.txt"
ok "Root files (setup.sh, build.sh, rebuild.sh, CMakeLists.txt)"

cp "$DOWNLOADS/PluginProcessor.h"   "$PROJECT/Source/PluginProcessor.h"
cp "$DOWNLOADS/PluginProcessor.cpp" "$PROJECT/Source/PluginProcessor.cpp"
cp "$DOWNLOADS/PluginEditor.h"      "$PROJECT/Source/PluginEditor.h"
cp "$DOWNLOADS/PluginEditor.cpp"    "$PROJECT/Source/PluginEditor.cpp"
ok "Core source (PluginProcessor.h/.cpp, PluginEditor.h/.cpp)"

cp "$DOWNLOADS/GlottalSource.h"    "$PROJECT/Source/DSP/GlottalSource.h"
cp "$DOWNLOADS/FormantFilter.h"    "$PROJECT/Source/DSP/FormantFilter.h"
cp "$DOWNLOADS/ChoirVoice.h"       "$PROJECT/Source/DSP/ChoirVoice.h"
cp "$DOWNLOADS/ChoirEngine.h"      "$PROJECT/Source/DSP/ChoirEngine.h"
cp "$DOWNLOADS/HumanizeEngine.h"   "$PROJECT/Source/DSP/HumanizeEngine.h"
cp "$DOWNLOADS/CaveReverb.h"       "$PROJECT/Source/DSP/CaveReverb.h"
cp "$DOWNLOADS/FormantShimmer.h"   "$PROJECT/Source/DSP/FormantShimmer.h"
ok "DSP files (GlottalSource, FormantFilter, ChoirVoice, ChoirEngine, HumanizeEngine, CaveReverb, FormantShimmer)"

cp "$DOWNLOADS/VowelTables.h"     "$PROJECT/Source/Data/VowelTables.h"
cp "$DOWNLOADS/IntervalTables.h"  "$PROJECT/Source/Data/IntervalTables.h"
cp "$DOWNLOADS/Presets.h"         "$PROJECT/Source/Data/Presets.h"
ok "Data files (VowelTables, IntervalTables, Presets)"

cp "$DOWNLOADS/UI.h"              "$PROJECT/Source/UI/UI.h"
cp "$DOWNLOADS/FormantScope.h"    "$PROJECT/Source/UI/FormantScope.h"
cp "$DOWNLOADS/VoicePanel.h"      "$PROJECT/Source/UI/VoicePanel.h"
cp "$DOWNLOADS/AncientPanel.h"    "$PROJECT/Source/UI/AncientPanel.h"
cp "$DOWNLOADS/FXPanel.h"         "$PROJECT/Source/UI/FXPanel.h"
ok "UI files (UI, FormantScope, VoicePanel, AncientPanel, FXPanel)"

# ═══════════════════════════════════════════════════════════════════════════════
step "Setting permissions"
# ═══════════════════════════════════════════════════════════════════════════════

chmod +x "$PROJECT/setup.sh"
chmod +x "$PROJECT/build.sh"
chmod +x "$PROJECT/rebuild.sh"
ok "setup.sh, build.sh, rebuild.sh are executable"

# ═══════════════════════════════════════════════════════════════════════════════
step "Initialising git"
# ═══════════════════════════════════════════════════════════════════════════════

cd "$PROJECT"

if [[ ! -d "$PROJECT/.git" ]]; then
    git init "$PROJECT"
    ok "git repository initialised"
else
    ok "git repository already exists"
fi

[[ -n "$(git config --global user.name 2>/dev/null)" ]] || \
    { git config --global user.name "Nicholas Bessmer"; ok "git user.name set"; }
[[ -n "$(git config --global user.email 2>/dev/null)" ]] || \
    { git config --global user.email "nicholas@tullytech.com"; ok "git user.email set"; }

# ═══════════════════════════════════════════════════════════════════════════════
step "Creating .gitignore"
# ═══════════════════════════════════════════════════════════════════════════════

cat > "$PROJECT/.gitignore" << 'GITIGNORE'
build/
cmake-build-*/
.DS_Store
*.xcuserdata/
DerivedData/
.vscode/
.idea/
JuceLibraryCode/
*.vst3
*.component
*.app
/tmp/av_build.log
/tmp/av_rebuild.log
GITIGNORE

ok ".gitignore created"

# ═══════════════════════════════════════════════════════════════════════════════
step "Downloading JUCE 8 (~1 minute)"
# ═══════════════════════════════════════════════════════════════════════════════

if [[ -f "$PROJECT/JUCE/CMakeLists.txt" ]]; then
    JUCE_VER=$(cat "$PROJECT/JUCE/VERSION" 2>/dev/null || echo "unknown")
    ok "JUCE $JUCE_VER already present — skipping download"
else
    echo "  Cloning from GitHub..."
    git submodule add https://github.com/juce-framework/JUCE.git JUCE || \
        git submodule update --init --recursive
    JUCE_VER=$(cat "$PROJECT/JUCE/VERSION" 2>/dev/null || echo "unknown")
    ok "JUCE $JUCE_VER downloaded"
fi

# ═══════════════════════════════════════════════════════════════════════════════
step "CMake configure"
# ═══════════════════════════════════════════════════════════════════════════════

GENERATOR="Xcode"
command -v xcodebuild &>/dev/null || { GENERATOR="Unix Makefiles"; warn "Using Makefile generator (Xcode not found)"; }

cmake -B "$PROJECT/build" \
    -G "$GENERATOR" \
    -DJUCE_BUILD_EXTRAS=OFF \
    -DJUCE_BUILD_EXAMPLES=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "$PROJECT" 2>&1 | grep -E "^(--|Configuring|Build files)" | head -10 || true

ok "CMake configured → $PROJECT/build"

# ═══════════════════════════════════════════════════════════════════════════════
step "Initial git commit"
# ═══════════════════════════════════════════════════════════════════════════════

cd "$PROJECT"
git add -A 2>/dev/null || true
if ! git diff --cached --quiet 2>/dev/null; then
    git commit -m "Initial commit — Ancient Voices source + JUCE submodule" 2>/dev/null && \
        ok "Initial commit created" || warn "Commit skipped"
else
    ok "Nothing to commit"
fi

# ═══════════════════════════════════════════════════════════════════════════════
sep
echo ""
echo -e "  ${GREEN}${BOLD}Setup complete!${RESET}"
echo ""
echo "  Now run the full build:"
echo ""
echo "    ${BOLD}$PROJECT/build.sh${RESET}"
echo ""
echo "  After that, for fast rebuilds when you change code:"
echo ""
echo "    ${BOLD}$PROJECT/rebuild.sh${RESET}"
echo ""
sep
echo ""
