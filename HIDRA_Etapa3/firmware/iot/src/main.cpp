#include "iot.h"
#if __has_include("secrets.h")
#include "secrets.h"
#define HIDRA_CONFIGURED 1
#else
#include "secrets.example.h"
#define HIDRA_CONFIGURED 1
#define HIDRA_TEMPLATE_CONFIG 1
#endif

#ifdef HIDRA_SYNTHETIC_DEMO
namespace {
uint32_t sessionCounter=0;
void syntheticSession() {
    if(!iot_boot_id()[0]) { Serial.println("Módulo não inicializado; configurar e verificar diagnóstico."); return; }
    // Somente ensaio de comunicação. Não é máquina de estados dos sensores.
    HidraEvent e;
    snprintf(e.session_id,sizeof(e.session_id),"%s_s%u",iot_boot_id(),++sessionCounter);
    e.sequence=1; strlcpy(e.data.trigger,"button",sizeof(e.data.trigger));
    if(!iot_publish(e)) { Serial.println("Evento fictício recusado; verificar IotStatus."); return; }
    for(int i=1;i<=5;++i) {
        e.type=EventType::StateChanged; e.state=static_cast<ActivityState>(i); e.sequence++;
        e.data=EventData{};
        snprintf(e.data.previous_state,sizeof(e.data.previous_state),"E%d",i-1);
        if(i==3) { e.data.has_mass=true; e.data.mass_g=185; }
        if(!iot_publish(e)) return;
    }
    e.type=EventType::Completed; e.data=EventData{}; e.sequence++;
    if(iot_publish(e)) {
        IotSnapshot s; strlcpy(s.session_id,e.session_id,sizeof(s.session_id)); s.state=e.state;
        iot_set_snapshot(s);
        Serial.println("Sessão FICTÍCIA aceita em memória; conferir sincronização no aplicativo.");
    }
}
}
#endif

void setup() {
    Serial.begin(115200);
#ifdef HIDRA_TEMPLATE_CONFIG
    Serial.println("HIDRA: configuração de exemplo vazia. Preencher secrets.h antes do ensaio.");
#endif
#ifdef HIDRA_CONFIGURED
    IotConfig config{HIDRA_DEVICE_ID, HIDRA_SSID, HIDRA_WIFI_PASSWORD,
        HIDRA_DATABASE_URL, HIDRA_API_KEY, HIDRA_EMAIL, HIDRA_PASSWORD, HIDRA_ROOT_CA};
    iot_begin(config);
    if(iot_get_status().last_error!=0) { Serial.println("Módulo IoT não inicializado; verificar configuração e LittleFS."); return; }
    // Na integração, a etapa 2 consulta iot_recovered_session() e emite Aborted/restart
    // com last_sequence + 1 ANTES de abrir uma nova atividade.
#ifdef HIDRA_SYNTHETIC_DEMO
    auto old=iot_recovered_session();
    if(old.open) {
        HidraEvent e; strlcpy(e.session_id,old.session_id,sizeof(e.session_id));
        e.sequence=old.last_sequence+1; e.type=EventType::Aborted; e.state=old.state;
        strlcpy(e.data.reason,"restart",sizeof(e.data.reason));
        if(!iot_publish(e)) Serial.println("Falha ao registrar interrupção; não iniciar sessão.");
    }
    Serial.println("Ensaio fictício: envie n pela Serial para gerar uma sessão normal.");
#endif
#else
    Serial.println("HIDRA: copiar secrets.example.h para secrets.h e configurar.");
#endif
}
void loop() {
#if defined(HIDRA_CONFIGURED) && defined(HIDRA_SYNTHETIC_DEMO)
    if(Serial.available() && Serial.read()=='n') syntheticSession();
#endif
    // A aquisição de sensores e a máquina de estados serão integradas pelas etapas 1 e 2.
    delay(1000);
}
