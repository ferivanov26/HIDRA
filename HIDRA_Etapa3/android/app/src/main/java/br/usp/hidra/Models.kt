package br.usp.hidra

data class Device(val id: String, val label: String)
data class AlertInfo(val id: String, val reason: String, val occurredAtMs: Long?)
data class DeviceStatus(
    val state: String = "E0", val sessionId: String? = null,
    val observedAtMs: Long? = null, val lastSeenMs: Long? = null,
    val pendingCount: Long = 0, val droppedCount: Long = 0,
    val activityActive: Boolean? = null
)
data class SessionSummary(
    val id: String, val order: Long, val result: String, val lastSequence: Long,
    val instructionCount: Long, val alertCount: Long, val activeAlerts: List<AlertInfo>,
    val startedAtMs: Long?, val endedAtMs: Long?, val estimatedIntakeMl: Double?,
    val historyIncomplete: Boolean
)
data class ActivityEvent(
    val id: String, val sequence: Long, val type: String, val state: String,
    val occurredAtMs: Long?, val details: String
) {
    val description: String get() = when(type) {
        "activity_started" -> "Atividade iniciada"
        "instruction_played" -> "Instrução verbal emitida"
        "caregiver_alert" -> "Auxílio do cuidador solicitado"
        "alert_resolved" -> "Condição de alerta resolvida"
        "activity_completed" -> "Sequência observável concluída"
        "activity_aborted" -> "Atividade interrompida"
        "state_changed" -> when(state) {
            "E1" -> "Copo retirado da posição inicial"
            "E2" -> "Copo posicionado para enchimento"
            "E3" -> "Aumento da massa do copo observado"
            "E4" -> "Copo retirado da plataforma de pesagem"
            "E5" -> "Encerramento observável"
            else -> "Estado alterado"
        }
        else -> "Evento desconhecido"
    }
}
fun resultLabel(value: String) = when(value) {
    "completed" -> "Sequência observável concluída"
    "aborted" -> "Interrompida"
    else -> "Em andamento"
}
