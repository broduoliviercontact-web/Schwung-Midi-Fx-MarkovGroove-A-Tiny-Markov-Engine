#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MODULE_ID="${MODULE_ID:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["id"])')}"
VERSION="${VERSION:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["version"])')}"
DIST="dist"

echo "-> Packaging ${MODULE_ID} v${VERSION}..."

# Build for aarch64 if needed
if [ ! -f build/aarch64/dsp.so ]; then
    echo "-> dsp.so not found, building..."
    ./scripts/build.sh
fi

# Stage
rm -rf "${DIST}/${MODULE_ID}"
mkdir -p "${DIST}/${MODULE_ID}"

cp src/module.json "${DIST}/${MODULE_ID}/module.json"
cp build/aarch64/dsp.so "${DIST}/${MODULE_ID}/dsp.so"
[ -f src/ui.js ]       && cp src/ui.js       "${DIST}/${MODULE_ID}/ui.js"
[ -f src/ui_chain.js ] && cp src/ui_chain.js "${DIST}/${MODULE_ID}/ui_chain.js"

# Archive — must extract to <id>/
TARBALL="${DIST}/${MODULE_ID}-module.tar.gz"
tar -C "${DIST}" -czf "${TARBALL}" "${MODULE_ID}"

echo "OK: ${TARBALL}"
echo ""
tar -tzf "${TARBALL}"
