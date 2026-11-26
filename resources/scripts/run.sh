#!/usr/bin/env bash

## Wrapper script for running the Cheetos using protontricks

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Determine if running from source tree (resources/scripts/) or release package (root)
if [ -f "$SCRIPT_DIR/../../config.json" ]; then
    PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
    CONFIG_FILE="$PROJECT_ROOT/config.json"
    BIN_DIR="$PROJECT_ROOT/build/bin"
else
    CONFIG_FILE="$SCRIPT_DIR/config.json"
    BIN_DIR="$SCRIPT_DIR"
fi

STEAM_APP_ID="3456950"

PROJECT_NAME="PG2-Cheetos"
if [ -f "$CONFIG_FILE" ]; then
    PARSED_NAME=$(grep -o '"project"[[:space:]]*:[[:space:]]*"[^"]*"' "$CONFIG_FILE" | sed -E 's/.*"project"[[:space:]]*:[[:space:]]*"([^"]*)".*/\1/' || true)
    if [ -n "$PARSED_NAME" ]; then
        PROJECT_NAME="$PARSED_NAME"
    fi
fi
DLL_NAME="${PROJECT_NAME}.dll"
INJECTOR_NAME="${PROJECT_NAME}_Injector.exe"

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m'

banner() {
    echo -e "${PURPLE}"
    echo "  ╔══════════════════════════════════════════════╗"
    echo "  ║       PIXEL GUN 2  CHEETOS                   ║"
    echo "  ║       Launcher ($(uname -s))                       ║"
    echo "  ╚══════════════════════════════════════════════╝"
    echo -e "${NC}"
}

info()    { echo -e "  ${CYAN}[*]${NC} $*"; }
success() { echo -e "  ${GREEN}[+]${NC} $*"; }
warn()    { echo -e "  ${YELLOW}[!]${NC} $*"; }
error()   { echo -e "  ${RED}[!]${NC} $*"; }

# --- Locate Steam game directory ---
find_game_dir() {
    local app_id="$STEAM_APP_ID"
    local steam_roots=(
        "$HOME/.local/share/Steam"
        "$HOME/.steam/steam"
        "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"
        "$HOME/Library/Application Support/Steam"
    )

    local libraries=()
    for root in "${steam_roots[@]}"; do
        if [ -d "$root" ]; then
            libraries+=("$root")
            local vdf="$root/steamapps/libraryfolders.vdf"
            if [ -f "$vdf" ]; then
                while IFS= read -r lib_path; do
                    if [ -n "$lib_path" ] && [ -d "$lib_path" ]; then
                        libraries+=("$lib_path")
                    fi
                done < <(grep -o '"path"[[:space:]]*"[^"]*"' "$vdf" | sed -E 's/.*"path"[[:space:]]*"([^"]*)".*/\1/' | sed 's/\\\\/\\/g' || true)
            fi
        fi
    done

    for lib in "${libraries[@]}"; do
        local manifest="$lib/steamapps/appmanifest_${app_id}.acf"
        if [ -f "$manifest" ]; then
            local install_dir
            install_dir=$(grep -o '"installdir"[[:space:]]*"[^"]*"' "$manifest" | sed -E 's/.*"installdir"[[:space:]]*"([^"]*)".*/\1/' || true)
            if [ -n "$install_dir" ] && [ -d "$lib/steamapps/common/$install_dir" ]; then
                echo "$lib/steamapps/common/$install_dir"
                return 0
            fi
        fi

        for candidate in "$lib/steamapps/common/Pixel Gun 2" "$lib/steamapps/common/Pixel Gun 2"; do
            if [ -d "$candidate" ]; then
                echo "$candidate"
                return 0
            fi
        done
    done

    return 1
}

# Check if actual Windows game executable is running under Wine/Proton
is_game_running() {
    pgrep -f -i "Pixel Gun 2\.exe" >/dev/null 2>&1
}

check_build() {
    if [ ! -f "$BIN_DIR/$DLL_NAME" ] && [ -f "$BIN_DIR/lib$DLL_NAME" ]; then
        info "Found lib$DLL_NAME, copying to $DLL_NAME for injector compatibility..."
        cp "$BIN_DIR/lib$DLL_NAME" "$BIN_DIR/$DLL_NAME"
    fi

    if [ ! -f "$BIN_DIR/$DLL_NAME" ]; then
        error "$DLL_NAME not found."
        if [[ "$BIN_DIR" == *"build/bin"* ]]; then
            echo "    Run build first: ./resources/scripts/build release --injector"
        fi
        exit 1
    fi

    if [ ! -f "$BIN_DIR/$INJECTOR_NAME" ]; then
        error "$INJECTOR_NAME not found."
        if [[ "$BIN_DIR" == *"build/bin"* ]]; then
            echo "    Run build first: ./resources/scripts/build release --injector"
        fi
        exit 1
    fi

    success "Binaries found"
}

# --- Launch via Wine / Protontricks ---
launch_wine() {
    cd "$BIN_DIR"
    local APP_ID="$STEAM_APP_ID"
    local OS_TYPE="$(uname -s)"

    if [[ "$OS_TYPE" == "Darwin" ]]; then
        # macOS handling
        warn "macOS detected. Protontricks is not used here."
        info "If you are using CrossOver or Whisky, you must run $INJECTOR_NAME manually"
        info "inside the exact same bottle/prefix where Pixel Gun 2 is running."
        echo ""
        info "Injector path: $BIN_DIR/$INJECTOR_NAME"
        exit 1
    fi

    # Check protontricks (Native or Flatpak)
    local PROTON_CMD=()
    if command -v protontricks &>/dev/null; then
        PROTON_CMD=(protontricks)
    elif command -v flatpak &>/dev/null && flatpak list --app 2>/dev/null | grep -q com.github.Matoking.protontricks; then
        PROTON_CMD=(flatpak run com.github.Matoking.protontricks)
    else
        error "protontricks is not installed."
        warn "It is required to inject into a Steam (Proton) game process."
        echo "    Arch Linux: sudo pacman -S protontricks"
        echo "    Flatpak:    flatpak install flathub com.github.Matoking.protontricks"
        exit 1
    fi

    echo ""
    info "Waiting for Pixel Gun 2.exe (timeout: 90s)..."

    local retries=0
    local max_retries=90
    while ! is_game_running; do
        sleep 1
        retries=$((retries + 1))
        if [ "$retries" -ge "$max_retries" ]; then
            error "Timed out waiting for game process. Exiting background task to prevent leak."
            exit 1
        fi
    done
    success "Game Detected"

    info "Waiting 3 seconds for Wine/Proton prefix stabilization..."
    sleep 3

    info "Launching $INJECTOR_NAME via Protontricks..."

    # Execute with Protontricks (--no-wait prevents interactive prompts or exit delays)
    "${PROTON_CMD[@]}" -c "wine \"./$INJECTOR_NAME\" --no-wait --timeout 60" "$APP_ID"
}

# --- Show game location ---
locate_game() {
    if game_dir=$(find_game_dir); then
        success "Game found at: $game_dir"
    else
        warn "Game not found in default Steam paths."
        echo ""
        info "Searched in:"
        echo "    ~/.local/share/Steam/steamapps/common/Pixel Gun 2"
        echo "    ~/.steam/steam/steamapps/common/Pixel Gun 2"
        echo "    ~/.var/app/com.valvesoftware.Steam/..."
    fi
}

# --- Show status ---
show_status() {
    info "Binaries:"
    if [ -f "$BIN_DIR/$DLL_NAME" ] || [ -f "$BIN_DIR/lib$DLL_NAME" ]; then
        success "  $DLL_NAME      ✓"
    else
        error "  $DLL_NAME      ✗ (Missing)"
    fi

    if [ -f "$BIN_DIR/$INJECTOR_NAME" ]; then
        success "  $INJECTOR_NAME    ✓"
    else
        error "  $INJECTOR_NAME    ✗ (Missing)"
    fi

    echo ""
    info "Wine:"
    if command -v wine &>/dev/null; then
        success "  wine installed   ✓  ($(wine --version 2>/dev/null || echo 'unknown version'))"
    else
        warn "  wine installed   ✗"
    fi

    echo ""
    info "Game location:"
    locate_game
}

# --- Main ---
main() {
    banner

    if [[ "${1:-}" == *"/steamapps/"* || "${1:-}" == *"proton"* || "${1:-}" == *"SteamLinuxRuntime"* || "${1:-}" == *"reaper"* ]]; then
        info "Steam Launch Option detected. Preparing environment..."
        check_build
        launch_wine &

        info "Starting game via exec..."
        exec "$@"
    fi

    case "${1:-inject}" in
        inject)
            check_build
            launch_wine
            ;;
        locate)
            locate_game
            ;;
        status)
            show_status
            ;;
        help|-h|--help)
            echo "  Usage: $0 [command]"
            echo ""
            echo "  Commands:"
            echo "    inject     — Launch injector via Wine (default)"
            echo "    locate     — Find Steam game installation"
            echo "    status     — Check build artifacts and dependencies"
            echo "    help       — Show this help message"
            echo ""
            echo "  Steam Launch Option:"
            echo "    /path/to/script.sh %command%"
            echo ""
            ;;
        *)
            error "Unknown command: $1"
            echo "  Run '$0 help' for usage."
            exit 1
            ;;
    esac
}

main "$@"
