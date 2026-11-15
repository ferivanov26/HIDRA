#!/usr/bin/env bash
# Uso: source scripts/activate_local_tools.sh
# Seleciona ferramentas locais já instaladas; não baixa dependências.
HIDRA_STAGE3_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$HIDRA_STAGE3_ROOT/.runtime/tools/venv/bin" ]]; then
    export PATH="$HIDRA_STAGE3_ROOT/.runtime/tools/venv/bin:$PATH"
fi
if [[ -d "$HIDRA_STAGE3_ROOT/.runtime/tools/node/bin" ]]; then
    export PATH="$HIDRA_STAGE3_ROOT/.runtime/tools/node/bin:$PATH"
fi
if [[ -x "$HIDRA_STAGE3_ROOT/.runtime/tools/jdk21/bin/java" ]]; then
    export JAVA_HOME="$HIDRA_STAGE3_ROOT/.runtime/tools/jdk21"
    export HIDRA_FIREBASE_JAVA_HOME="$JAVA_HOME"
    export HIDRA_ANDROID_JAVA_HOME="$JAVA_HOME"
    export PATH="$JAVA_HOME/bin:$PATH"
fi
if [[ -d "$HIDRA_STAGE3_ROOT/.runtime/tools/platformio" ]]; then
    export PLATFORMIO_CORE_DIR="$HIDRA_STAGE3_ROOT/.runtime/tools/platformio"
fi
if [[ -d "$HIDRA_STAGE3_ROOT/.runtime/tools/android-sdk" ]]; then
    export ANDROID_HOME="$HIDRA_STAGE3_ROOT/.runtime/tools/android-sdk"
    export ANDROID_USER_HOME="$HIDRA_STAGE3_ROOT/.runtime/tools/android-user"
    export GRADLE_USER_HOME="$HIDRA_STAGE3_ROOT/.runtime/tools/gradle"
    export PATH="$ANDROID_HOME/platform-tools:$PATH"
fi
