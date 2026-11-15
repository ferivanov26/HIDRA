#!/usr/bin/env bash
# HIDRA - executa os testes da Etapa 2 no computador (Linux/macOS/WSL, g++ >= 9).
# 1) Cenarios do orquestrador (codigo do firmware + simulador fisico)
# 2) Teste de fumaca do firmware completo (.ino) com mocks de Arduino/ESP32
set -euo pipefail
cd "$(dirname "$0")"
S=../HIDRA_Etapa2/src
OUT=${TMPDIR:-/tmp}
CXX=${CXX:-g++}

echo "### 1) Cenarios do orquestrador"
$CXX -std=c++17 -Wall -Wextra -O1 -o "$OUT/hidra_test_orq" test_orquestrador.cpp \
  $S/contrato/HidraEvent.cpp $S/orquestrador/*.cpp
"$OUT/hidra_test_orq"

echo; echo "### 2) Firmware completo com mocks (fase a: sessao; fase b: reinicio)"
$CXX -std=gnu++17 -Wall -Wextra -O0 -Imocks -include mocks/relogio_mock.h -o "$OUT/hidra_smoke" \
  smoke_firmware.cpp $S/hardware/HidraHardware.cpp $S/contrato/HidraEvent.cpp \
  $S/iot/HidraIotStub.cpp $S/orquestrador/*.cpp $S/plataforma/*.cpp
rm -f /tmp/hidra_smoke_estado.txt
"$OUT/hidra_smoke" a
"$OUT/hidra_smoke" b
echo; echo "TODOS OS TESTES PASSARAM"
