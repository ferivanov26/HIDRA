#include "iot.h"
#include <cassert>
#include <vector>
using namespace HidraIoT;
TestSerial Serial;
static IotStatus status;
static RecoveredSession oldSession;
static std::vector<HidraEvent> accepted;
static std::vector<IotSnapshot> snapshots;
static bool rejectRecovery=false;
static bool rejectTransitions=false;
void HidraIoT::iot_begin(const IotConfig&) {}
bool HidraIoT::iot_publish(const HidraEvent& e) {
    if(rejectRecovery && e.type==EventType::Aborted) return false;
    if(rejectTransitions && e.type==EventType::StateChanged) return false;
    accepted.push_back(e); return true;
}
IotStatus HidraIoT::iot_get_status() { return status; }
void HidraIoT::iot_set_snapshot(const IotSnapshot& s) { snapshots.push_back(s); }
RecoveredSession HidraIoT::iot_recovered_session() { return oldSession; }
const char* HidraIoT::iot_boot_id() { return "testboot"; }
void setup();
void loop();
void pump() { for(int i=0;i<16;++i) loop(); }
int main(int argc,char** argv) {
    assert(argc==2);
    const std::string mode=argv[1];
    rejectTransitions=mode=="backpressure";
    if(mode=="uninitialized") status.last_error=-5;
    if(mode=="recovery" || mode=="recovery_retry") {
        oldSession.open=true; oldSession.last_sequence=4; oldSession.state=ActivityState::E2;
        strlcpy(oldSession.session_id,"old_s1",sizeof(oldSession.session_id));
        rejectRecovery=mode=="recovery_retry";
    }
    setup();
    Serial.input="n"; pump();
    if(mode=="uninitialized") { assert(accepted.empty()); return 0; }
    if(mode=="recovery_retry") {
        assert(accepted.empty());
        rejectRecovery=false;
        Serial.input="n"; pump();
    }
    if(mode=="backpressure") {
        assert(accepted.size()==1);
        Serial.input="n"; pump();
        assert(accepted.size()==1);
        rejectTransitions=false;
        pump();
    }
    const bool recovered=oldSession.open;
    assert(accepted.size()==(recovered?8:7));
    const size_t start=recovered?1:0;
    if(recovered) {
        assert(accepted[0].type==EventType::Aborted);
        assert(accepted[0].sequence==5);
        assert(std::strcmp(accepted[0].session_id,"old_s1")==0);
        assert(std::strcmp(accepted[0].data.reason,"restart")==0);
    }
    assert(accepted[start].type==EventType::Started);
    for(size_t i=start;i<accepted.size();++i) assert(accepted[i].sequence==i-start+1);
    assert(accepted.back().type==EventType::Completed);
    assert(snapshots.back().state==ActivityState::E5);
    Serial.input="n"; pump();
    assert(std::strcmp(accepted.back().session_id,accepted[start].session_id)!=0);
}
