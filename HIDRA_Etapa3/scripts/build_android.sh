#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -n "${HIDRA_ANDROID_JAVA_HOME:-}" ]]; then
    export JAVA_HOME="$HIDRA_ANDROID_JAVA_HOME"
fi
bash android/gradlew -p "$PWD/android" :app:assembleDemoDebug :app:assembleEmulatorDebug :app:assembleCloudDebug --console=plain "$@"
