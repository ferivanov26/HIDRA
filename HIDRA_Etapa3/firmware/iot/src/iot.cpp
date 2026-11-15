#include "iot.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <cmath>
#include <time.h>
#include <vector>
#include <algorithm>

namespace HidraIoT {
namespace {
constexpr size_t MAX_EVENT = 1024, CAPACITY = 200;
const char* TYPES[] = {"activity_started", "state_changed", "instruction_played", "caregiver_alert", "alert_resolved", "activity_completed", "activity_aborted"};
struct Queued {
    HidraEvent event;
    uint32_t counter;
    uint64_t uptime;
    int64_t occurred;
};
IotConfig cfg{};
QueueHandle_t queue = nullptr;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
IotStatus status;
IotSnapshot snapshot;
int64_t snapshotObservedAt = -1;
RecoveredSession recovered;
char boot[33]{};
uint32_t counter = 0;
String token, refreshToken;
uint64_t expiresAt = 0;
std::vector<String> pending;
bool ready = false, incomplete = false;
uint32_t sessionOrder = 0;
uint32_t journalOrder = 0;

uint64_t monotonicMs() { return esp_timer_get_time() / 1000ULL; }
bool utcValid() { return time(nullptr) > 1700000000; }
String stateName(ActivityState s) { return "E" + String(static_cast<int>(s)); }
bool key(const char* s, size_t capacity = 64) {
    if(!s) return false;
    size_t n = strnlen(s, capacity);
    if (!n || n >= capacity) return false;
    for (size_t i=0; i<n; ++i) if (!isalnum(static_cast<unsigned char>(s[i])) && s[i]!='_' && s[i]!='-') return false;
    return true;
}
void error(int code) { portENTER_CRITICAL(&mux); status.last_error=code; portEXIT_CRITICAL(&mux); }
void loss() { portENTER_CRITICAL(&mux); status.dropped_count++; status.last_error=-2; portEXIT_CRITICAL(&mux); }
bool valid(const HidraEvent& e) {
    if(e.schema_version!=1 || !key(e.session_id) || !e.sequence || static_cast<int>(e.type)>6 || static_cast<int>(e.state)>5) return false;
    const auto& d=e.data;
    if(strnlen(d.trigger,sizeof(d.trigger))==sizeof(d.trigger) || strnlen(d.reason,sizeof(d.reason))==sizeof(d.reason)) return false;
    if(d.has_mass && (!std::isfinite(d.mass_g) || d.mass_g<0)) return false;
    if(d.has_estimated_intake && (!std::isfinite(d.estimated_intake_ml) || d.estimated_intake_ml<0)) return false;
    // Recusar campos incompatíveis, em vez de descartá-los na serialização.
    if(d.has_session_order && (e.type!=EventType::Started || d.session_order==0)) return false;
    if(d.has_mass && e.type!=EventType::StateChanged) return false;
    if(d.has_estimated_intake && e.type!=EventType::Completed) return false;
    if(d.previous_state[0] && e.type!=EventType::StateChanged) return false;
    if(d.trigger[0] && e.type!=EventType::Started) return false;
    if(d.instruction_id[0] && e.type!=EventType::InstructionPlayed) return false;
    if(d.alert_id[0] && e.type!=EventType::CaregiverAlert && e.type!=EventType::AlertResolved) return false;
    if(d.reason[0] && e.type!=EventType::CaregiverAlert && e.type!=EventType::AlertResolved && e.type!=EventType::Aborted) return false;
    if(d.attempt && e.type!=EventType::InstructionPlayed && e.type!=EventType::CaregiverAlert) return false;
    switch(e.type) {
        case EventType::Started: return e.state==ActivityState::E0 && (strcmp(d.trigger,"button")==0 || strcmp(d.trigger,"schedule")==0);
        case EventType::StateChanged: return strnlen(d.previous_state,3)==2 && d.previous_state[0]=='E' && d.previous_state[1]>='0' && d.previous_state[1]<='5';
        case EventType::InstructionPlayed: return key(d.instruction_id) && d.attempt>0;
        case EventType::CaregiverAlert: return key(d.alert_id) && key(d.reason) && d.attempt>0;
        case EventType::AlertResolved: return key(d.alert_id) && key(d.reason);
        case EventType::Completed: return e.state==ActivityState::E5;
        case EventType::Aborted: return strcmp(d.reason,"restart")==0 || strcmp(d.reason,"local_cancel")==0;
    }
    return false;
}
uint32_t crc(const String& text) {
    uint32_t value=0xffffffff;
    for(size_t i=0;i<text.length();++i) {
        value^=static_cast<uint8_t>(text[i]);
        for(int b=0;b<8;++b) value=(value>>1)^(0xedb88320u & (0u-(value&1)));
    }
    return ~value;
}
bool writeRecord(const String& path, const JsonDocument& doc) {
    String content; serializeJson(doc,content);
    String temp=path+".tmp";
    File f=LittleFS.open(temp,"w");
    if(!f) return false;
    uint32_t size=content.length(), checksum=crc(content);
    bool ok=f.write(reinterpret_cast<uint8_t*>(&size),4)==4 && f.write(reinterpret_cast<uint8_t*>(&checksum),4)==4 && f.print(content)==size;
    f.flush(); f.close();
    return ok && LittleFS.rename(temp,path);
}
bool readRecord(const String& path, JsonDocument& doc) {
    File f=LittleFS.open(path,"r");
    if(!f) return false;
    uint32_t size=0,checksum=0;
    if(f.read(reinterpret_cast<uint8_t*>(&size),4)!=4 || f.read(reinterpret_cast<uint8_t*>(&checksum),4)!=4 || size>65536 || f.size()!=size+8) { f.close(); return false; }
    String content=f.readString(); f.close();
    return content.length()==size && crc(content)==checksum && !deserializeJson(doc,content);
}
void serialize(const Queued& q, JsonDocument& doc) {
    auto& e=q.event; const auto& d=e.data;
    doc["schema_version"]=1; doc["device_id"]=cfg.device_id;
    doc["session_id"]=e.session_id; doc["sequence"]=e.sequence;
    doc["event_id"]=String(boot)+"_e"+String(q.counter);
    doc["boot_id"]=boot; doc["uptime_ms"]=q.uptime;
    if(q.occurred>=0) doc["occurred_at_ms"]=q.occurred;
    doc["type"]=TYPES[static_cast<int>(e.type)]; doc["state"]=stateName(e.state);
    switch(e.type) {
        case EventType::Started:
            doc["data"]["trigger"]=d.trigger;
            if(d.has_session_order) doc["data"]["session_order"]=d.session_order;
            break;
        case EventType::StateChanged:
            doc["data"]["previous_state"]=d.previous_state;
            if(d.has_mass) doc["data"]["mass_g"]=d.mass_g;
            break;
        case EventType::InstructionPlayed:
            doc["data"]["instruction_id"]=d.instruction_id; doc["data"]["attempt"]=d.attempt; break;
        case EventType::CaregiverAlert:
            doc["data"]["attempt"]=d.attempt;
            doc["data"]["alert_id"]=d.alert_id; doc["data"]["reason"]=d.reason; break;
        case EventType::AlertResolved:
            doc["data"]["alert_id"]=d.alert_id; doc["data"]["reason"]=d.reason; break;
        case EventType::Completed:
            if(d.has_estimated_intake) doc["data"]["estimated_intake_ml"]=d.estimated_intake_ml;
            break;
        case EventType::Aborted: doc["data"]["reason"]=d.reason; break;
    }
}
int http(const String& url, const char* method, const String& body, String& response, String* etag=nullptr, const String& match="", bool form=false) {
    WiFiClientSecure tls;
    tls.setCACert(cfg.root_ca); tls.setTimeout(5000); tls.setHandshakeTimeout(5);
    HTTPClient client;
    client.setTimeout(5000); client.setConnectTimeout(5000);
    if(!client.begin(tls,url)) return -10;
    const char* headers[]={"ETag"}; client.collectHeaders(headers,1);
    client.addHeader("Content-Type",form?"application/x-www-form-urlencoded":"application/json");
    if(etag) client.addHeader("X-Firebase-ETag","true");
    if(match.length()) client.addHeader("if-match",match);
    int code=client.sendRequest(method,body);
    if(code<0) {
        char detail[128]{};
        const int tlsCode=tls.lastError(detail,sizeof(detail));
        Serial.printf("IoT HTTPS: http=%d tls=%d (%s), heap=%u\n",code,tlsCode,detail,ESP.getFreeHeap());
    }
    if(code>0) { response=client.getString(); if(etag) *etag=client.header("ETag"); }
    client.end();
    if(code==401) expiresAt=0;
    return code;
}
bool authenticate() {
    if(token.length() && monotonicMs()<expiresAt) return true;
    String response,body; JsonDocument input,output;
    int code=0;
    if(refreshToken.length()) {
        String encoded;
        const char* hex="0123456789ABCDEF";
        for(size_t i=0;i<refreshToken.length();++i) {
            unsigned char c=refreshToken[i];
            if(isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') encoded+=static_cast<char>(c);
            else { encoded+='%'; encoded+=hex[c>>4]; encoded+=hex[c&15]; }
        }
        body="grant_type=refresh_token&refresh_token="+encoded;
        code=http(String("https://securetoken.googleapis.com/v1/token?key=")+cfg.api_key,"POST",body,response,nullptr,"",true);
        if(code==200 && !deserializeJson(output,response)) {
            token=output["id_token"].as<String>(); refreshToken=output["refresh_token"].as<String>();
            expiresAt=monotonicMs()+(output["expires_in"].as<uint32_t>()-60)*1000ULL;
            return token.length()>0;
        }
    }
    input["email"]=cfg.device_email; input["password"]=cfg.device_password; input["returnSecureToken"]=true;
    serializeJson(input,body);
    code=http(String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=")+cfg.api_key,"POST",body,response);
    if(code!=200 || deserializeJson(output,response)) { error(code); return false; }
    token=output["idToken"].as<String>(); refreshToken=output["refreshToken"].as<String>();
    expiresAt=monotonicMs()+(output["expiresIn"].as<uint32_t>()-60)*1000ULL;
    return token.length()>0;
}
String base() { String result=cfg.database_url; while(result.endsWith("/")) result.remove(result.length()-1); return result; }
int database(const String& path, const char* method, const JsonDocument* value, String& response, String* etag=nullptr, const String& match="") {
    String body; if(value) serializeJson(*value,body);
    return http(base()+"/devices/"+cfg.device_id+"/"+path+".json?auth="+token,method,body,response,etag,match);
}
String summaryPath(const String& session) { return "/s_"+session+".bin"; }
bool summaryFor(const JsonDocument& event, JsonDocument& summary) {
    if(iot_get_status().dropped_count>0) incomplete=true;
    String session=event["session_id"].as<String>();
    String path=summaryPath(session);
    bool exists=LittleFS.exists(path);
    if(exists && !readRecord(path,summary)) { error(-12); return false; }
    uint32_t seq=event["sequence"];
    uint32_t last=summary["last_sequence"] | 0;
    if(last>=seq) return true; // Snapshot já salvo antes de reiniciar: não contar de novo.
    if(!exists) {
        JsonDocument meta;
        if(!readRecord("/meta.bin",meta)) { error(-12); return false; }
        uint32_t order=event["data"]["session_order"] | 0;
        if(!order) order=(meta["session_order"] | 0)+1;
        meta["session_order"]=std::max<uint32_t>(meta["session_order"] | 0,order);
        if(!writeRecord("/meta.bin",meta)) return false;
        portENTER_CRITICAL(&mux); sessionOrder=meta["session_order"].as<uint32_t>(); portEXIT_CRITICAL(&mux);
        summary["session_order"]=order; summary["result"]="in_progress";
        summary["instruction_count"]=0; summary["alert_count"]=0;
        summary["history_incomplete"]=incomplete;
    }
    if(seq!=last+1 || incomplete) summary["history_incomplete"]=true;
    summary["last_sequence"]=seq; summary["last_state"]=event["state"];
    String kind=event["type"].as<String>();
    auto d=event["data"];
    if(kind=="activity_started" && event["occurred_at_ms"].is<int64_t>()) summary["started_at_ms"]=event["occurred_at_ms"];
    if(kind=="instruction_played") summary["instruction_count"]=(summary["instruction_count"] | 0)+1;
    if(kind=="caregiver_alert") {
        summary["alert_count"]=(summary["alert_count"] | 0)+1;
        String id=d["alert_id"].as<String>();
        summary["active_alerts"][id]["reason"]=d["reason"];
        if(event["occurred_at_ms"].is<int64_t>()) summary["active_alerts"][id]["occurred_at_ms"]=event["occurred_at_ms"];
    }
    if(kind=="alert_resolved") {
        summary["active_alerts"].remove(d["alert_id"].as<String>());
        if(summary["active_alerts"].size()==0) summary.remove("active_alerts");
    }
    if(kind=="activity_completed" || kind=="activity_aborted") {
        summary["result"]=kind=="activity_completed"?"completed":"aborted";
        if(event["occurred_at_ms"].is<int64_t>()) summary["ended_at_ms"]=event["occurred_at_ms"];
        if(d["estimated_intake_ml"].is<float>()) summary["estimated_intake_ml"]=d["estimated_intake_ml"];
    }
    return true;
}
int publish(const JsonDocument& event) {
    String session=event["session_id"].as<String>();
    String path="sessions/"+session+"/events/"+event["event_id"].as<String>();
    String response,etag; JsonDocument stored;
    int code=database(path,"GET",nullptr,response,&etag);
    if(code!=200) return code;
    if(deserializeJson(stored,response)) return -11;
    if(!stored.isNull()) {
        if(stored["payload"].as<JsonVariantConst>() != event.as<JsonVariantConst>()) return -13;
    } else {
        JsonDocument record;
        record["payload"]=event.as<JsonVariantConst>(); record["received_at_ms"][".sv"]="timestamp";
        code=database(path,"PUT",&record,response,nullptr,etag);
        if(code!=200) return code;
    }
    JsonDocument summary;
    if(!summaryFor(event,summary)) return -12;
    // Snapshot local preparado antes do envio remoto: reutilizado após falha/reinício.
    if(!writeRecord(summaryPath(session),summary)) return -12;
    code=database("sessions/"+session+"/summary","PUT",&summary,response);
    if(code!=200) return code;
    return 200;
}
void presence() {
    IotSnapshot current; IotStatus st; int64_t observed;
    portENTER_CRITICAL(&mux); current=snapshot; st=status; observed=snapshotObservedAt; portEXIT_CRITICAL(&mux);
    JsonDocument doc;
    doc["boot_id"]=boot; doc["state"]=stateName(current.state);
    if(current.session_id[0]) doc["session_id"]=current.session_id;
    if(observed>=0) doc["observed_at_ms"]=observed;
    if(current.has_details) {
        doc["activity_active"]=current.activity_active;
        doc["sequence"]=current.sequence; doc["attempt"]=current.attempt;
        doc["active_alert_count"]=current.active_alert_count;
        doc["mass_available"]=current.mass_available;
        if(current.mass_available && std::isfinite(current.mass_g)) doc["mass_g"]=current.mass_g;
    }
    doc["last_seen_ms"][".sv"]="timestamp";
    doc["pending_count"]=st.pending_count; doc["dropped_count"]=st.dropped_count;
    String response; int code=database("status","PUT",&doc,response);
    if(code!=200) error(code);
}
void recoverReference(const JsonDocument& e) {
    const char* kind=e["type"] | "";
    strlcpy(recovered.session_id,e["session_id"] | "",sizeof(recovered.session_id));
    recovered.last_sequence=e["sequence"] | 0;
    const char* state=e["state"] | "E0";
    recovered.state=static_cast<ActivityState>(state[1]-'0');
    recovered.open=strcmp(kind,"activity_completed")!=0 && strcmp(kind,"activity_aborted")!=0;
}
void worker(void*) {
    uint64_t nextNetwork=0,lastPresence=0,nextWifi=0;
    uint32_t backoff=2, persistedLoss=UINT32_MAX;
    Queued q;
    for(;;) {
        while(xQueueReceive(queue,&q,0)==pdTRUE) {
            if(pending.size()>=CAPACITY) { loss(); incomplete=true; continue; }
            JsonDocument event; serialize(q,event);
            if(measureJson(event)>MAX_EVENT) { loss(); error(-3); continue; }
            JsonDocument metadata;
            if(!readRecord("/meta.bin",metadata)) { loss(); error(-12); continue; }
            metadata["journal_order"]=++journalOrder;
            const uint32_t producerOrder=event["data"]["session_order"] | 0;
            metadata["session_order"]=std::max<uint32_t>(metadata["session_order"] | 0,producerOrder);
            portENTER_CRITICAL(&mux); sessionOrder=metadata["session_order"].as<uint32_t>(); portEXIT_CRITICAL(&mux);
            if(!writeRecord("/meta.bin",metadata)) { loss(); error(-12); continue; }
            char number[12]; snprintf(number,sizeof(number),"%010u",journalOrder);
            String path="/q_"+String(number)+".bin";
            if(!writeRecord(path,event)) { loss(); incomplete=true; continue; }
            pending.push_back(path);
            JsonDocument ref; ref["event"]=event.as<JsonVariantConst>();
            if(!writeRecord("/open.bin",ref)) error(-12);
        }
        IotStatus local=iot_get_status();
        if(local.dropped_count!=persistedLoss) {
            JsonDocument meta;
            if(readRecord("/meta.bin",meta)) {
                meta["dropped_count"]=local.dropped_count;
                if(writeRecord("/meta.bin",meta)) persistedLoss=local.dropped_count;
            }
        }
        bool connected=WiFi.status()==WL_CONNECTED;
        bool authenticated=false;
        if(!connected && monotonicMs()>=nextWifi) { WiFi.begin(cfg.ssid,cfg.wifi_password); nextWifi=monotonicMs()+10000; }
        if(connected && utcValid() && monotonicMs()>=nextNetwork) {
            authenticated=authenticate();
            if(!authenticated) {
                nextNetwork=monotonicMs()+backoff*1000+esp_random()%1000;
                backoff=std::min<uint32_t>(backoff*2,60);
            }
        }
        portENTER_CRITICAL(&mux);
        status.wifi=connected; status.authenticated=authenticated;
        status.pending_count=pending.size()+uxQueueMessagesWaiting(queue);
        portEXIT_CRITICAL(&mux);
        if(authenticated && monotonicMs()-lastPresence>=30000) { presence(); lastPresence=monotonicMs(); }
        if(authenticated && !pending.empty() && monotonicMs()>=nextNetwork) {
            JsonDocument event;
            int code=readRecord(pending.front(),event)?publish(event):-13;
            if(code==200 || code==400 || code==403 || code==-13) {
                if(code!=200) {
                    incomplete=true; error(code);
                    LittleFS.rename(pending.front(),pending.front()+".bad");
                } else LittleFS.remove(pending.front());
                pending.erase(pending.begin()); backoff=2;
            } else {
                error(code); nextNetwork=monotonicMs()+backoff*1000+esp_random()%1000;
                backoff=std::min<uint32_t>(backoff*2,60);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

void iot_begin(const IotConfig& config) {
    if(ready) return;
    cfg=config;
    if(!key(cfg.device_id) || !cfg.ssid || !strlen(cfg.ssid) || !cfg.wifi_password || !cfg.database_url || !cfg.api_key || !strlen(cfg.api_key) || !cfg.device_email || !strlen(cfg.device_email) || !cfg.device_password || !strlen(cfg.device_password) || !String(cfg.database_url).startsWith("https://") || !cfg.root_ca || !strlen(cfg.root_ca)) { error(-4); return; }
    // Não formatar automaticamente: isso apagaria o diário após falha de montagem.
    if(!LittleFS.begin(false)) { error(-5); return; }
    if(cfg.boot_id) {
        if(!key(cfg.boot_id,sizeof(boot))) { error(-4); return; }
        strlcpy(boot,cfg.boot_id,sizeof(boot));
    } else snprintf(boot,sizeof(boot),"%08x%08x",esp_random(),esp_random());
    JsonDocument meta;
    if(LittleFS.exists("/meta.bin")) {
        if(!readRecord("/meta.bin",meta)) { error(-12); return; }
    } else {
        meta["session_order"]=0; meta["dropped_count"]=0; meta["journal_order"]=0;
        if(!writeRecord("/meta.bin",meta)) { error(-12); return; }
    }
    sessionOrder=meta["session_order"] | 0;
    journalOrder=meta["journal_order"] | 0;
    status.dropped_count=meta["dropped_count"] | 0;
    incomplete=status.dropped_count>0;
    JsonDocument reference;
    if(LittleFS.exists("/open.bin")) {
        if(!readRecord("/open.bin",reference)) { error(-12); return; }
        JsonDocument e; e.set(reference["event"]); recoverReference(e);
    }
    File root=LittleFS.open("/"); File file=root.openNextFile();
    while(file) {
        String name=file.name(); if(!name.startsWith("/")) name="/"+name;
        if(name.startsWith("/q_") && name.endsWith(".bin")) pending.push_back(name);
        file=root.openNextFile();
    }
    // Ordinal do diário reservado em Flash antes da gravação: ordem entre boots sem UTC.
    std::sort(pending.begin(),pending.end(),[](const String& a,const String& b) { return a<b; });
    for(const auto& path: pending) {
        JsonDocument event;
        if(readRecord(path,event)) {
            recoverReference(event);
            sessionOrder=std::max<uint32_t>(sessionOrder,event["data"]["session_order"] | 0);
        }
    }
    queue=xQueueCreate(32,sizeof(Queued));
    if(!queue) { error(-6); return; }
    WiFi.mode(WIFI_STA); configTime(0,0,"pool.ntp.org","time.google.com");
    ready=xTaskCreate(worker,"hidra_iot",16384,nullptr,1,nullptr)==pdPASS;
    if(!ready) error(-7);
}
bool iot_publish(const HidraEvent& event) {
    if(!ready || !valid(event)) { error(-1); return false; }
    Queued q{}; q.event=event; q.uptime=monotonicMs();
    q.occurred=utcValid()?static_cast<int64_t>(time(nullptr))*1000:-1;
    portENTER_CRITICAL(&mux); q.counter=++counter; portEXIT_CRITICAL(&mux);
    if(xQueueSend(queue,&q,0)!=pdTRUE) { loss(); return false; }
    return true;
}
IotStatus iot_get_status() { portENTER_CRITICAL(&mux); auto result=status; portEXIT_CRITICAL(&mux); return result; }
void iot_set_snapshot(const IotSnapshot& value) {
    if(static_cast<int>(value.state)>5 || (value.session_id[0] && !key(value.session_id))) return;
    int64_t observed=utcValid()?static_cast<int64_t>(time(nullptr))*1000:-1;
    portENTER_CRITICAL(&mux); snapshot=value; snapshotObservedAt=observed; portEXIT_CRITICAL(&mux);
}
RecoveredSession iot_recovered_session() { return recovered; }
const char* iot_boot_id() { return boot; }

uint32_t iot_last_session_order() { portENTER_CRITICAL(&mux); auto order=sessionOrder; portEXIT_CRITICAL(&mux); return order; }
} // namespace HidraIoT
