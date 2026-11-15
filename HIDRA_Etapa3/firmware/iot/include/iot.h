#pragma once
#include <Arduino.h>

enum class EventType : uint8_t { Started, StateChanged, InstructionPlayed, CaregiverAlert, AlertResolved, Completed, Aborted };
enum class ActivityState : uint8_t { E0, E1, E2, E3, E4, E5 };
struct EventData {
    char previous_state[3] = "";
    char trigger[16] = "";
    char instruction_id[64] = "";
    char alert_id[64] = "";
    char reason[64] = "";
    uint32_t attempt = 0;
    bool has_mass = false;
    float mass_g = 0;
    bool has_estimated_intake = false;
    float estimated_intake_ml = 0;
};
struct HidraEvent {
    uint8_t schema_version = 1;
    char session_id[64] = "";
    uint32_t sequence = 0;
    EventType type = EventType::Started;
    ActivityState state = ActivityState::E0;
    EventData data;
};
struct IotConfig {
    const char* device_id;
    const char* ssid;
    const char* wifi_password;
    const char* database_url;
    const char* api_key;
    const char* device_email;
    const char* device_password;
    const char* root_ca; // PEM da CA que valida os endpoints; nunca usar setInsecure.
};
struct IotStatus {
    bool wifi = false;
    bool authenticated = false;
    uint32_t pending_count = 0;
    uint32_t dropped_count = 0;
    int last_error = 0;
};
struct IotSnapshot {
    char session_id[64] = "";
    ActivityState state = ActivityState::E0;
};
struct RecoveredSession {
    char session_id[64] = "";
    uint32_t last_sequence = 0;
    ActivityState state = ActivityState::E0;
    bool open = false;
};
void iot_begin(const IotConfig& config);
bool iot_publish(const HidraEvent& event);
IotStatus iot_get_status();
void iot_set_snapshot(const IotSnapshot& snapshot);
RecoveredSession iot_recovered_session();
const char* iot_boot_id();
