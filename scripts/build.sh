#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-auto}"
MODULE_ID="${MODULE_ID:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["id"])')}"
DOCKER_IMAGE="${DOCKER_IMAGE:-${MODULE_ID}-builder}"

build_docker() {
    echo "-> Building with Docker..."
    docker build -f scripts/Dockerfile -t "${DOCKER_IMAGE}" .
    mkdir -p build/aarch64
    docker run --rm -v "$(pwd)/build/aarch64:/out" "${DOCKER_IMAGE}" \
        cp /build/build/aarch64/dsp.so /out/dsp.so
    echo "OK: build/aarch64/dsp.so"
}

build_native() {
    echo "-> Building with native cross-compiler..."

    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        CC_CROSS=aarch64-linux-gnu-gcc
    elif command -v aarch64-linux-musl-gcc >/dev/null 2>&1; then
        CC_CROSS=aarch64-linux-musl-gcc
    elif command -v zig >/dev/null 2>&1; then
        CC_CROSS="zig cc -target aarch64-linux-gnu"
    else
        echo "No aarch64 cross-compiler found."
        echo "Use Docker Desktop, install aarch64 musl cross tools, or install zig."
        exit 1
    fi

    make aarch64 CC_CROSS="$CC_CROSS"
    echo "OK: build/aarch64/dsp.so"
}

case "$MODE" in
    docker)
        build_docker
        ;;
    native)
        build_native
        ;;
    auto)
        if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 || \
           command -v aarch64-linux-musl-gcc >/dev/null 2>&1 || \
           command -v zig >/dev/null 2>&1; then
            build_native
        elif command -v docker >/dev/null 2>&1; then
            build_docker
        else
            build_native
        fi
        ;;
    *)
        echo "Usage: $0 [docker|native|auto]"
        exit 1
        ;;
esac
