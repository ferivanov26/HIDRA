#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -n "${HIDRA_FIREBASE_JAVA_HOME:-}" ]]; then
    export JAVA_HOME="$HIDRA_FIREBASE_JAVA_HOME"
    export PATH="$JAVA_HOME/bin:$PATH"
fi
if [[ ! -x firebase/node_modules/.bin/firebase ]]; then
    echo "Dependências Firebase ausentes. Execute: npm --prefix firebase ci" >&2
    exit 1
fi
python3 -m unittest tests.test_simulator -v
env -u DEBUG npm --prefix firebase test
env -u DEBUG firebase/node_modules/.bin/firebase emulators:exec --config firebase/firebase.json \
    --only auth,database --project demo-hidra \
    'HIDRA_TEST_EMULATOR=1 python3 -m unittest tests.test_rest_emulator -v'
