#pragma once
#include "firebase_ca.h"
// Copiar para secrets.h (ignorado pelo Git). Sem esses dados, o exemplo só inicia a Serial.
static const char HIDRA_DEVICE_ID[] = "hidra_01";
static const char HIDRA_SSID[] = "";
static const char HIDRA_WIFI_PASSWORD[] = "";
static const char HIDRA_DATABASE_URL[] = "https://SEU-BANCO.firebasedatabase.app";
static const char HIDRA_API_KEY[] = "";
static const char HIDRA_EMAIL[] = "";
static const char HIDRA_PASSWORD[] = "";
// Bundle público oficial do Google. Atualizar se a cadeia dos endpoints mudar.
static const char* HIDRA_ROOT_CA = HIDRA_GOOGLE_ROOTS;
