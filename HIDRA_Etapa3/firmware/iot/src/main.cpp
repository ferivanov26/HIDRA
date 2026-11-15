#include "iot.h"
#ifndef HIDRA_NETWORK_PROBE
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
bool recoveryPending=false;
bool initialized=false;
bool demoActive=false;
uint8_t demoStep=0;
char demoSession[64]{};
bool recoverSession() {
    if(!recoveryPending) return true;
    const auto old=iot_recovered_session();
    HidraEvent e; strlcpy(e.session_id,old.session_id,sizeof(e.session_id));
    e.sequence=old.last_sequence+1; e.type=EventType::Aborted; e.state=old.state;
    strlcpy(e.data.reason,"restart",sizeof(e.data.reason));
    if(!iot_publish(e)) return false;
    IotSnapshot s; strlcpy(s.session_id,e.session_id,sizeof(s.session_id)); s.state=e.state;
    iot_set_snapshot(s);
    recoveryPending=false;
    return true;
}
void syntheticSession() {
    if(!initialized || !recoverSession()) { Serial.println("Sessão bloqueada: inicialização ou recuperação pendente."); return; }
    if(demoActive) { Serial.println("Sessão fictícia ainda pendente; aguarde a fila."); return; }
    // Somente ensaio de comunicação. Não é máquina de estados dos sensores.
    snprintf(demoSession,sizeof(demoSession),"%s_s%u",iot_boot_id(),++sessionCounter);
    demoStep=0;
    demoActive=true;
}
void advanceDemo() {
    if(!demoActive) return;
    HidraEvent e;
    strlcpy(e.session_id,demoSession,sizeof(e.session_id));
    e.sequence=demoStep+1;
    if(demoStep==0) strlcpy(e.data.trigger,"button",sizeof(e.data.trigger));
    else if(demoStep<=5) {
        e.type=EventType::StateChanged; e.state=static_cast<ActivityState>(demoStep);
        snprintf(e.data.previous_state,sizeof(e.data.previous_state),"E%d",demoStep-1);
        if(demoStep==3) { e.data.has_mass=true; e.data.mass_g=185; }
    } else {
        e.type=EventType::Completed; e.state=ActivityState::E5;
    }
    // Em saturação, manter o mesmo evento/sequence e tentar no próximo loop.
    if(!iot_publish(e)) return;
    IotSnapshot s; strlcpy(s.session_id,e.session_id,sizeof(s.session_id)); s.state=e.state;
    iot_set_snapshot(s);
    if(++demoStep==7) {
        demoActive=false;
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
    initialized=true;
    recoveryPending=iot_recovered_session().open;
    if(!recoverSession()) Serial.println("Recuperação pendente; novas sessões bloqueadas.");
    Serial.println("Ensaio fictício: envie n pela Serial para gerar uma sessão normal.");
#endif
#else
    Serial.println("HIDRA: copiar secrets.example.h para secrets.h e configurar.");
#endif
}
void loop() {
#if defined(HIDRA_CONFIGURED) && defined(HIDRA_SYNTHETIC_DEMO)
    if(initialized && recoverSession()) advanceDemo();
    if(Serial.available()) {
        const char command=Serial.read();
        if(command=='n') syntheticSession();
        if(command=='s') {
            const auto s=iot_get_status();
            Serial.printf("wifi=%d auth=%d pending=%u dropped=%u error=%d\n",s.wifi,s.authenticated,s.pending_count,s.dropped_count,s.last_error);
        }
    }
#endif
    // A aquisição de sensores e a máquina de estados serão integradas pelas etapas 1 e 2.
    delay(10);
}
#endif
