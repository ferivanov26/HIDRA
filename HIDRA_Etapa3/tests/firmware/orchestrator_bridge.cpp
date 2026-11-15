#include "iot.h"
#include "iot/HidraIot.h"
#include "orquestrador/Orquestrador.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
HidraIoT::HidraEvent received;
HidraIoT::IotSnapshot current;
std::string boot;
uint32_t eventCounter=0;
bool accepting=true;
}
namespace HidraIoT {
void iot_begin(const IotConfig& config) { boot=config.boot_id; }
bool iot_publish(const HidraEvent& event) { received=event; return accepting; }
IotStatus iot_get_status() { IotStatus s; s.wifi=true; s.authenticated=true; return s; }
void iot_set_snapshot(const IotSnapshot& snapshot) { current=snapshot; }
RecoveredSession iot_recovered_session() { return {}; }
uint32_t iot_last_session_order() { return 37; }
}
struct Outputs: Saidas {
    SessaoPersistida saved{};
    uint32_t order=37;
    void tocarFaixa(uint8_t,uint32_t) override {}
    void registrarLocal(const char*) override {}
    void salvarSessaoAberta(const SessaoPersistida& s) override { saved=s; }
    void encerrarSessaoPersistida() override {}
    uint32_t proximaOrdemSessao() override { return ++order; }
    void statusAlterado(const HidraStatusSnapshot& s) override { iot_update_status(s); }
    void publicarEvento(const HidraEvent& e) override {
        assert(::iot_publish(e));
        assert(received.sequence==e.sequence && std::strcmp(received.session_id,e.session_id)==0);
        assert(static_cast<int>(received.type)==static_cast<int>(e.type));
        assert(static_cast<int>(received.state)==static_cast<int>(e.state));
        assert(received.data.has_session_order==e.data.has_session_order);
        if(e.data.has_session_order) assert(received.data.session_order==e.data.session_order);
        if(e.data.has_trigger) assert(std::strcmp(received.data.trigger,hidra_trigger_name(e.data.trigger))==0);
        if(e.data.has_instruction_id) assert(std::strcmp(received.data.instruction_id,e.data.instruction_id)==0);
        if(e.data.has_reason) assert(std::strcmp(received.data.reason,e.data.reason)==0);
        if(e.data.has_alert_id) assert(std::strcmp(received.data.alert_id,e.data.alert_id)==0);
        if(e.data.has_attempt) assert(received.data.attempt==e.data.attempt);
        if(e.data.has_mass_g) assert(received.data.has_mass && received.data.mass_g==e.data.mass_g);
        if(e.data.has_estimated_intake_ml) assert(received.data.has_estimated_intake && received.data.estimated_intake_ml==e.data.estimated_intake_ml);
        if(e.data.has_previous_state) assert(std::strcmp(received.data.previous_state,hidra_state_name(e.data.previous_state))==0);
        char id[64],json[1024];
        snprintf(id,sizeof(id),"%s_e%u",boot.c_str(),++eventCounter);
        assert(hidra_event_to_json(e,"sim_hidra_01",id,boot.c_str(),1800000000000LL+eventCounter*1000,eventCounter*1000,json,sizeof(json)));
        std::puts(json);
    }
};
int main(int argc,char** argv) {
    const std::string scenario=argc>1?argv[1]:"normal";
    hidra_set_boot_counter(65537);
    ::iot_begin({"sim_hidra_01","","","","","",""});
    assert(boot==hidra_boot_id() && boot=="10001");
    assert(::iot_last_session_order()==37);
    const auto connected=::iot_get_status(); assert(connected.wifi_connected && connected.authenticated);
    ParametrosAtividade p; parametrosPadrao(p);
    p.timeout_etapa_ms[0]=100; p.repeticao_ms=100; p.max_tentativas=1;
    Outputs out; Orquestrador orchestrator(p,out);
    EstadoFisico physical{}; physical.copo_na_base=true; physical.massa_disponivel=true;
    orchestrator.iniciarAtividade(StartTrigger::Button,physical,0);
    assert(current.has_details && current.activity_active && current.sequence==2);
    if(scenario=="alert") {
        orchestrator.tick(150,physical);
        orchestrator.tick(300,physical);
        assert(current.active_alert_count>0);
    }
    physical.copo_na_base=false;
    orchestrator.processar({Observacao::CopoSaiuDaBase,500,0},physical);
    if(scenario=="restart") {
        hidra_set_boot_counter(65538); ::iot_begin({"sim_hidra_01","","","","","",""});
        Orquestrador recovered(p,out); recovered.recuperarSessao(out.saved);
        assert(!current.activity_active);
        assert(boot=="10002");
        return 0;
    }
    physical.plataforma_ocupada=true; physical.massa_estavel=true;
    physical.massa_estavel_g=100; physical.massa_atual_g=100;
    orchestrator.processar({Observacao::PlataformaOcupada,1000,100},physical);
    physical.massa_estavel_g=300; physical.massa_atual_g=300;
    orchestrator.processar({Observacao::MassaEstavel,2000,300},physical);
    physical.plataforma_ocupada=false;
    orchestrator.processar({Observacao::PlataformaVazia,3000,0},physical);
    physical.plataforma_ocupada=true; physical.massa_atual_g=150; physical.massa_estavel_g=150;
    if(scenario=="base") {
        physical.copo_na_base=true; physical.plataforma_ocupada=false;
        orchestrator.processar({Observacao::CopoVoltouABase,10000,0},physical);
    } else orchestrator.processar({Observacao::PlataformaOcupada,10000,150},physical);
    assert(!orchestrator.ativo() && !current.activity_active);
    assert(orchestrator.estado()==ActivityState::E5);
    assert(current.active_alert_count==0 && current.sequence==received.sequence);
    assert(current.mass_available);
    if(scenario=="base") assert(!received.data.has_estimated_intake);
    else assert(received.data.has_estimated_intake && received.data.estimated_intake_ml==150);
}
