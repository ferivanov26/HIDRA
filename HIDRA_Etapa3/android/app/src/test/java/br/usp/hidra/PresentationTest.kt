package br.usp.hidra

import org.junit.Test
import org.junit.Assert.*

class PresentationTest {
    @Test fun restartExplainsMissingClockWithoutInventingOccurrenceTime() {
        val event=ActivityEvent("boot_e4",4,"activity_aborted","E1",null,"reason=restart",
            mapOf("reason" to "restart"),1794718453662L)
        assertEquals("O dispositivo reiniciou e encerrou a atividade anterior.",event.readableDetails())
        assertTrue(event.timeLabel().startsWith("Recebido em"))
        assertTrue(event.timeLabel().contains("horário do evento não sincronizado"))
        assertNull(event.occurredAtMs)
    }
    @Test fun localCancelAndAlertResolutionHaveDistinctDescriptions() {
        fun event(type: String, reason: String)=ActivityEvent("id",1,type,"E2",null,"",mapOf("reason" to reason))
        assertTrue(event("activity_aborted","local_cancel").readableDetails().contains("cancelada no dispositivo"))
        assertTrue(event("alert_resolved","progress").readableDetails().contains("voltou a avançar"))
        assertTrue(event("caregiver_alert","sensor_unavailable").readableDetails().contains("sensor de massa"))
    }
    @Test fun completedWithoutVolumeDoesNotSuggestAnEstimate() {
        val event=ActivityEvent("id",1,"activity_completed","E5",null,"")
        assertTrue(event.readableDetails().contains("Não há estimativa"))
        assertFalse(event.readableDetails().contains("mL"))
    }
    @Test fun instructionsAndMassArePresentedWithLabelsAndUnits() {
        val event=ActivityEvent("id",1,"instruction_played","E2",null,"",mapOf("instruction_id" to "add_water","attempt" to 2L))
        assertEquals("Adicionar água ao copo. Tentativa 2.",event.readableDetails())
        val mass=event.copy(type="state_changed",state="E3",data=mapOf("mass_g" to 185.5))
        assertEquals("Massa registrada: 185,5 g.",mass.readableDetails())
    }
    @Test fun unknownResultIsNotPresentedAsActive() {
        assertEquals("Resultado não informado",resultLabel("invalid"))
    }
}
