#ifdef HIDRA_NETWORK_PROBE
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <time.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Diagnóstico da infraestrutura. Não publica sessões nem comanda atuadores.
void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.printf("HIDRA NETWORK PROBE chip=%s revision=%u flash=%u\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getFlashChipSize());
    const bool mounted=LittleFS.begin(false);
    Serial.printf("littlefs=%s (sem formatação automática)\n",mounted?"ok":"não preparado");
    if(mounted) LittleFS.end();
    if(!HIDRA_SSID[0]) { Serial.println("wifi=sem configuração local"); return; }
    WiFi.mode(WIFI_STA);
    WiFi.begin(HIDRA_SSID,HIDRA_WIFI_PASSWORD);
    const uint32_t start=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-start<30000) delay(100);
    if(WiFi.status()!=WL_CONNECTED) { Serial.println("wifi=timeout"); return; }
    Serial.printf("wifi=ok rssi=%d\n",WiFi.RSSI());
    configTime(0,0,"pool.ntp.org","time.google.com");
    const uint32_t clockStart=millis();
    while(time(nullptr)<1700000000 && millis()-clockStart<30000) delay(100);
    if(time(nullptr)<1700000000) { Serial.println("utc=timeout; TLS não executado"); return; }
    Serial.println("utc=ok");
    WiFiClientSecure tls;
    tls.setCACert(HIDRA_ROOT_CA);
    tls.setHandshakeTimeout(5);
    HTTPClient http;
    http.setConnectTimeout(5000); http.setTimeout(5000);
    if(!http.begin(tls,"https://identitytoolkit.googleapis.com/")) {
        Serial.println("tls=falha na inicialização"); return;
    }
    const int code=http.GET();
    // Um HTTP 404 também comprova TLS: a raiz não é uma operação de autenticação.
    Serial.printf("tls=%s http=%d\n",code>0?"ok":"falha",code);
    http.end();
    Serial.println("Fim do diagnóstico; autenticação e RTDB exigem projeto configurado.");
}
void loop() { delay(100); }
#endif
