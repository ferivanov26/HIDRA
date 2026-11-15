package br.usp.hidra

import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

fun formatDate(ms: Long?): String = ms?.let {
    DateTimeFormatter.ofPattern("dd/MM/yyyy 'às' HH:mm:ss").withZone(ZoneId.systemDefault()).format(Instant.ofEpochMilli(it))
} ?: "Horário não registrado"

fun stateLabel(state: String) = when(state) {
    "E0" -> "Início da atividade"
    "E1" -> "Copo fora da base"
    "E2" -> "Copo na plataforma"
    "E3" -> "Água adicionada ao copo"
    "E4" -> "Copo fora da plataforma"
    "E5" -> "Etapas concluídas"
    else -> "Estado não reconhecido"
}
fun reasonLabel(reason: String) = when(reason) {
    "restart" -> "O dispositivo reiniciou e encerrou a atividade anterior."
    "local_cancel" -> "A atividade foi cancelada no dispositivo."
    "no_start" -> "A atividade não foi iniciada no tempo esperado."
    "stalled", "timeout" -> "A etapa não avançou no tempo esperado."
    "cup_not_returned" -> "O copo ainda não foi devolvido."
    "deviation" -> "Foi observada uma mudança fora da sequência esperada."
    "sensor_unavailable" -> "O sensor de massa está indisponível."
    "progress" -> "A atividade voltou a avançar."
    "caregiver_ack" -> "O auxílio foi reconhecido no dispositivo."
    "sensor_recovered" -> "O sensor voltou a responder."
    "session_closed" -> "O alerta foi encerrado junto com a atividade."
    else -> "Consulte os detalhes deste registro."
}
fun instructionLabel(id: String) = when(id) {
    "start" -> "Iniciar a atividade"
    "take_cup" -> "Retirar o copo da base"
    "place_cup" -> "Colocar o copo na plataforma"
    "add_water", "fill_cup" -> "Adicionar água ao copo"
    "remove_cup" -> "Retirar o copo da plataforma"
    "return_cup" -> "Devolver o copo"
    "completed" -> "Conclusão da atividade"
    "ask_help" -> "Solicitar auxílio"
    else -> "Instrução do dispositivo"
}
fun formatAmount(value: Double) = String.format(Locale.forLanguageTag("pt-BR"), "%.1f", value)
fun ActivityEvent.readableDetails(): String = when(type) {
    "activity_started" -> when(data["trigger"]) {
        "button" -> "Iniciada pelo botão ou console do dispositivo."
        "schedule" -> "Iniciada pelo horário programado."
        else -> "Uma nova atividade foi registrada."
    }
    "instruction_played" -> instructionLabel(data["instruction_id"] as? String ?: "") +
        ((data["attempt"] as? Number)?.let { ". Tentativa ${it.toLong()}." } ?: ".")
    "caregiver_alert", "alert_resolved", "activity_aborted" -> reasonLabel(data["reason"] as? String ?: "")
    "state_changed" -> (data["mass_g"] as? Number)?.let { "Massa registrada: ${formatAmount(it.toDouble())} g." }
        ?: "${stateLabel(state)}."
    "activity_completed" -> (data["estimated_intake_ml"] as? Number)?.let {
        "Variação de massa equivalente a ${formatAmount(it.toDouble())} mL."
    } ?: "A sequência foi encerrada. Não há estimativa de volume neste registro."
    else -> "Registro recebido do dispositivo."
}
fun ActivityEvent.timeLabel(): String = when {
    occurredAtMs != null -> formatDate(occurredAtMs)
    receivedAtMs != null -> "Recebido em ${formatDate(receivedAtMs)} · horário do evento não sincronizado"
    else -> "Horário do evento não registrado; a ordem foi preservada."
}

fun countLabel(count: Long, singular: String, plural: String) = "$count ${if(count==1L) singular else plural}"
