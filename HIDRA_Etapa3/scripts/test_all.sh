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
if ! command -v g++ >/dev/null; then
    echo "Instale g++ para executar os testes do programa C++ de demonstração." >&2
    exit 1
fi
python3 -m unittest tests.test_simulator tests.test_firmware_demo tests.test_orchestrator_bridge -v
env -u DEBUG npm --prefix firebase test
env -u DEBUG firebase/node_modules/.bin/firebase emulators:exec --config firebase/firebase.json \
    --only auth,database --project demo-hidra \
    'HIDRA_TEST_EMULATOR=1 python3 -m unittest tests.test_rest_emulator -v'
