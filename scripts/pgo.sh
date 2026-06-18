#!/usr/bin/env bash
# pgo.sh — Two-phase PGO build for DropAndDrag on macOS.
#
# Usage:
#   ./scripts/pgo.sh          — full pipeline (instrument → profile → optimise)
#   ./scripts/pgo.sh profile  — only step 1+2 (rebuild instrumented + run)
#   ./scripts/pgo.sh use      — only step 3 (optimised build from existing .profdata)
#
# Requires: cmake, ninja, llvm-profdata (Xcode CLT or Homebrew llvm)
#
# Output: build_pgo/DropAndDrag.app  — PGO-optimised release build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
PROFRAW="$ROOT/build_pgo_instr/default.profraw"
PROFDATA="$ROOT/default.profdata"
SKIA_DIR="${SKIA_DIR:-$HOME/skia}"

phase="${1:-all}"

# ── Step 1: instrumented build ─────────────────────────────────────────────
if [[ "$phase" == "all" || "$phase" == "profile" ]]; then
    echo "▶ Step 1: instrumented build"
    cmake -B "$ROOT/build_pgo_instr" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIA_DIR="$SKIA_DIR" \
        -DDROPANDDRAG_ENABLE_PGO_GENERATE=ON \
        -DDROPANDDRAG_BUILD_TESTS=OFF
    cmake --build "$ROOT/build_pgo_instr" --parallel

    # ── Step 2: profile collection ─────────────────────────────────────────
    echo ""
    echo "▶ Step 2: run the instrumented build and exercise the app."
    echo "   Recommended workflow (~30 seconds):"
    echo "   • Shake the mouse vigorously 5 times to open the shelf"
    echo "   • Drop 20+ files onto the shelf"
    echo "   • Drag the shelf window across the screen"
    echo "   • Scroll through the items"
    echo "   • Clear the shelf"
    echo "   • Quit the app (Cmd+Q or tray → Quit)"
    echo ""
    echo "   Launching now..."
    LLVM_PROFILE_FILE="$PROFRAW" \
        open -W "$ROOT/build_pgo_instr/DropAndDrag.app"

    # ── Step 2b: merge raw profiles ────────────────────────────────────────
    echo "▶ Merging profile data..."
    if ! command -v llvm-profdata &>/dev/null; then
        # Try Homebrew LLVM if system clang doesn't expose llvm-profdata
        LLVM_PROFDATA="$(brew --prefix llvm 2>/dev/null)/bin/llvm-profdata"
        if [[ ! -x "$LLVM_PROFDATA" ]]; then
            echo "ERROR: llvm-profdata not found. Install with: brew install llvm"
            exit 1
        fi
    else
        LLVM_PROFDATA=llvm-profdata
    fi

    "$LLVM_PROFDATA" merge -output="$PROFDATA" "$PROFRAW"
    echo "   Profile written: $PROFDATA"
fi

# ── Step 3: PGO-optimised build ────────────────────────────────────────────
if [[ "$phase" == "all" || "$phase" == "use" ]]; then
    if [[ ! -f "$PROFDATA" ]]; then
        echo "ERROR: $PROFDATA not found. Run './scripts/pgo.sh profile' first."
        exit 1
    fi
    echo "▶ Step 3: PGO-optimised build"
    cmake -B "$ROOT/build_pgo" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIA_DIR="$SKIA_DIR" \
        -DDROPANDDRAG_ENABLE_PGO_USE=ON \
        -DDROPANDDRAG_PGO_PROFILE="$PROFDATA" \
        -DDROPANDDRAG_BUILD_TESTS=OFF
    cmake --build "$ROOT/build_pgo" --parallel
    echo ""
    echo "✓ PGO build complete: $ROOT/build_pgo/DropAndDrag.app"
fi
