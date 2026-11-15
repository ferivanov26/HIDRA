#ifdef HIDRA_INTEGRATED_ORCHESTRATOR
#include "iot.h"
#include "iot/HidraIot.h"
#include "firebase_ca.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
bool copyKey(char* target,size_t capacity,const char* source,size_t sourceCapacity) {
    const size_t length=strnlen(source,sourceCapacity);
    if(length==sourceCapacity || length>=capacity) return false;
    memcpy(target,source,length+1);
    return true;
}
}
void iot_begin(const IotConfig& config) {
    HidraIoT::IotConfig transport{config.device_id,config.wifi_ssid,config.wifi_password,
        config.database_url,config.api_key,config.device_email,config.device_password,
        HIDRA_GOOGLE_ROOTS,hidra_boot_id()};
    HidraIoT::iot_begin(transport);
}
bool iot_publish(const HidraEvent& event) {
    if(static_cast<uint8_t>(event.type)>6 || static_cast<uint8_t>(event.state)>5) return false;
    HidraIoT::HidraEvent converted;
    converted.schema_version=event.schema_version;
    if(!copyKey(converted.session_id,sizeof(converted.session_id),event.session_id,sizeof(event.session_id))) return false;
    converted.sequence=event.sequence;
    switch(event.type) {
        case EventType::ActivityStarted: converted.type=HidraIoT::EventType::Started; break;
        case EventType::StateChanged: converted.type=HidraIoT::EventType::StateChanged; break;
        case EventType::InstructionPlayed: converted.type=HidraIoT::EventType::InstructionPlayed; break;
        case EventType::CaregiverAlert: converted.type=HidraIoT::EventType::CaregiverAlert; break;
        case EventType::AlertResolved: converted.type=HidraIoT::EventType::AlertResolved; break;
        case EventType::ActivityCompleted: converted.type=HidraIoT::EventType::Completed; break;
        case EventType::ActivityAborted: converted.type=HidraIoT::EventType::Aborted; break;
    }
    converted.state=static_cast<HidraIoT::ActivityState>(event.state);
    const auto& src=event.data; auto& dst=converted.data;
    if(src.has_previous_state) {
        if(static_cast<uint8_t>(src.previous_state)>5) return false;
        dst.previous_state[0]='E';
        dst.previous_state[1]=static_cast<char>('0'+static_cast<uint8_t>(src.previous_state));
        dst.previous_state[2]='\0';
    }
    if(src.has_trigger) {
        if(static_cast<uint8_t>(src.trigger)>1) return false;
        strlcpy(dst.trigger,hidra_trigger_name(src.trigger),sizeof(dst.trigger));
    }
    if(src.has_instruction_id && !copyKey(dst.instruction_id,sizeof(dst.instruction_id),src.instruction_id,sizeof(src.instruction_id))) return false;
    if(src.has_alert_id && !copyKey(dst.alert_id,sizeof(dst.alert_id),src.alert_id,sizeof(src.alert_id))) return false;
    if(src.has_reason && !copyKey(dst.reason,sizeof(dst.reason),src.reason,sizeof(src.reason))) return false;
    if(src.has_attempt) { if(!src.attempt) return false; dst.attempt=src.attempt; }
    dst.has_mass=src.has_mass_g; dst.mass_g=src.mass_g;
    dst.has_estimated_intake=src.has_estimated_intake_ml; dst.estimated_intake_ml=src.estimated_intake_ml;
    dst.has_session_order=src.has_session_order; dst.session_order=src.session_order;
    return HidraIoT::iot_publish(converted);
}
IotStatus iot_get_status() {
    const auto current=HidraIoT::iot_get_status();
    IotStatus result{};
    result.wifi_connected=current.wifi; result.authenticated=current.authenticated;
    result.pending_count=current.pending_count; result.dropped_count=current.dropped_count;
    if(current.last_error) snprintf(result.last_error,sizeof(result.last_error),"codigo %d",current.last_error);
    return result;
}
void iot_update_status(const HidraStatusSnapshot& value) {
    HidraIoT::IotSnapshot converted;
    if(!copyKey(converted.session_id,sizeof(converted.session_id),value.session_id,sizeof(value.session_id))) return;
    converted.state=static_cast<HidraIoT::ActivityState>(value.state);
    converted.has_details=true; converted.activity_active=value.activity_active;
    converted.sequence=value.sequence; converted.attempt=value.attempt;
    converted.active_alert_count=value.active_alerts;
    converted.mass_available=value.mass_available;
    converted.mass_g=value.mass_g;
    HidraIoT::iot_set_snapshot(converted);
}
uint32_t iot_last_session_order() { return HidraIoT::iot_last_session_order(); }
IotRecoveredSession iot_recovered_session() {
    const auto stored=HidraIoT::iot_recovered_session();
    IotRecoveredSession result{};
    strlcpy(result.session_id,stored.session_id,sizeof(result.session_id));
    result.last_sequence=stored.last_sequence; result.open=stored.open;
    result.state=static_cast<ActivityState>(stored.state);
    return result;
}
#endif
