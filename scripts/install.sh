#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MOVE_HOST="${1:-move.local}"
MOVE_USER="root"
MOVE_MODULES_DIR="/data/UserData/schwung/modules"
MODULE_CATEGORY="midi_fx"
MODULE_ID="${MODULE_ID:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["id"])')}"
MODULE_NAME="${MODULE_NAME:-$(python3 -c 'import json; data = json.load(open("src/module.json", "r", encoding="utf-8")); print(data.get("name", data["id"]))')}"
DEST="${MOVE_USER}@${MOVE_HOST}:${MOVE_MODULES_DIR}/${MODULE_CATEGORY}/${MODULE_ID}"
DSO="build/aarch64/dsp.so"

if [ ! -f "$DSO" ]; then
    echo "Missing $DSO. Run ./scripts/build.sh first."
    exit 1
fi

echo "-> Deploying ${MODULE_NAME} (${MODULE_ID}) to ${MOVE_HOST}..."

ssh "${MOVE_USER}@${MOVE_HOST}" "mkdir -p ${MOVE_MODULES_DIR}/${MODULE_CATEGORY}/${MODULE_ID}"
scp src/module.json "${DEST}/module.json"
scp "$DSO" "${DEST}/dsp.so"
if [ -f src/ui.js ]; then
    scp src/ui.js "${DEST}/ui.js"
fi
if [ -f src/ui_chain.js ]; then
    scp src/ui_chain.js "${DEST}/ui_chain.js"
fi

echo ""
echo "OK: installed to ${MOVE_MODULES_DIR}/${MODULE_CATEGORY}/${MODULE_ID}/"
echo ""
echo "If ${MODULE_NAME} does not appear immediately, restart Schwung:"
echo "ssh ${MOVE_USER}@${MOVE_HOST} 'pkill -x schwung || true; sleep 1; nohup /data/UserData/schwung/schwung >/tmp/schwung.log 2>&1 </dev/null &'"
