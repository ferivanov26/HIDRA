package br.usp.hidra

import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.*
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.tasks.await

interface AuthRepository {
    val signedIn: Boolean
    suspend fun signIn(email: String, password: String)
    fun signOut()
}
class FirebaseAuthRepository(private val auth: FirebaseAuth): AuthRepository {
    override val signedIn get() = auth.currentUser != null
    override suspend fun signIn(email: String, password: String) { auth.signInWithEmailAndPassword(email, password).await() }
    override fun signOut() = auth.signOut()
}
class DemoAuthRepository: AuthRepository {
    override var signedIn = false; private set
    override suspend fun signIn(email: String, password: String) { signedIn = true }
    override fun signOut() { signedIn = false }
}
interface HidraRepository {
    suspend fun devices(): List<Device>
    fun status(device: String): Flow<DeviceStatus?>
    fun summary(device: String, session: String): Flow<SessionSummary?>
    fun history(device: String, before: Long? = null): Flow<List<SessionSummary>>
    fun events(device: String, session: String): Flow<List<ActivityEvent>>
}
private fun <T> Query.observe(parse: (DataSnapshot) -> T): Flow<T> = callbackFlow {
    val listener = object: ValueEventListener {
        override fun onDataChange(snapshot: DataSnapshot) {
            try { trySend(parse(snapshot)) } catch(e: Exception) { close(e) }
        }
        override fun onCancelled(error: DatabaseError) { close(error.toException()) }
    }
    addValueEventListener(listener)
    awaitClose { removeEventListener(listener) }
}
private fun DataSnapshot.text(name: String) = child(name).getValue(String::class.java)
private fun DataSnapshot.number(name: String) = (child(name).value as? Number)?.toLong()
private fun DataSnapshot.summary(): SessionSummary? {
    if (!exists()) return null
    return SessionSummary(key.orEmpty(), number("session_order") ?: error("Resumo sem ordem"),
        text("result") ?: "in_progress", number("last_sequence") ?: 0,
        number("instruction_count") ?: 0, number("alert_count") ?: 0,
        child("active_alerts").children.map { AlertInfo(it.key.orEmpty(), it.text("reason").orEmpty(), it.number("occurred_at_ms")) },
        number("started_at_ms"), number("ended_at_ms"), (child("estimated_intake_ml").value as? Number)?.toDouble(),
        child("history_incomplete").value == true)
}
class FirebaseHidraRepository(private val db: FirebaseDatabase, private val auth: FirebaseAuth): HidraRepository {
    private fun device(id: String) = db.getReference("devices").child(id)
    override suspend fun devices(): List<Device> {
        val uid = auth.currentUser?.uid ?: error("Acesso necessário")
        val links = db.getReference("caregiverDevices").child(uid).get().await()
        return links.children.filter { it.value == true }.map {
            val id = it.key ?: error("Vínculo inválido")
            Device(id, device(id).child("metadata/label").get().await().getValue(String::class.java) ?: id)
        }
    }
    override fun status(device: String) = device(device).child("status").observe { s ->
        if (!s.exists()) null else DeviceStatus(s.text("state") ?: "E0", s.text("session_id"),
            s.number("observed_at_ms"), s.number("last_seen_ms"), s.number("pending_count") ?: 0, s.number("dropped_count") ?: 0)
    }
    override fun summary(device: String, session: String) = device(device).child("sessions").child(session).child("summary")
        .observe { it.summary()?.copy(id = session) }
    override fun history(device: String, before: Long?): Flow<List<SessionSummary>> {
        var q: Query = device(device).child("sessions").orderByChild("summary/session_order")
        if (before != null) q = q.endBefore(before.toDouble())
        return q.limitToLast(20).observe { s ->
            s.children.mapNotNull { it.child("summary").summary()?.copy(id = it.key.orEmpty()) }.sortedByDescending { it.order }
        }
    }
    override fun events(device: String, session: String) = device(device).child("sessions").child(session).child("events")
        .orderByChild("payload/sequence").observe { snapshot -> snapshot.children.map {
            val p = it.child("payload")
            ActivityEvent(it.key.orEmpty(), p.number("sequence") ?: 0, p.text("type").orEmpty(),
                p.text("state").orEmpty(), p.number("occurred_at_ms"), p.child("data").value?.toString().orEmpty())
        }.sortedBy { it.sequence } }
}
class FakeHidraRepository: HidraRepository {
    private val now = System.currentTimeMillis()
    private val normal = SessionSummary("demo_s1", 1, "completed", 7, 0, 0, emptyList(), now - 120000, now - 60000, null, false)
    private val alert = SessionSummary("demo_s2", 2, "in_progress", 5, 1, 1,
        listOf(AlertInfo("demo_a1", "timeout", now - 10000)), now - 30000, null, null, false)
    override suspend fun devices() = listOf(Device("demo_hidra", "HIDRA — demonstração fictícia"))
    override fun status(device: String) = flowOf(DeviceStatus("E2", "demo_s2", now, now, 0, 0))
    override fun summary(device: String, session: String) = flowOf(if(session == alert.id) alert else normal)
    override fun history(device: String, before: Long?) = flowOf(listOf(alert, normal).filter { before == null || it.order < before })
    override fun events(device: String, session: String): Flow<List<ActivityEvent>> {
        val values = if(session == alert.id) listOf(
            ActivityEvent("d1", 1, "activity_started", "E0", now - 30000, "trigger=button"),
            ActivityEvent("d2", 2, "state_changed", "E1", now - 25000, "previous_state=E0"),
            ActivityEvent("d3", 3, "state_changed", "E2", now - 20000, "previous_state=E1"),
            ActivityEvent("d4", 4, "instruction_played", "E2", now - 15000, "instruction_id=fill_cup, attempt=1"),
            ActivityEvent("d5", 5, "caregiver_alert", "E2", now - 10000, "alert_id=demo_a1, reason=timeout, attempt=3"))
        else listOf(ActivityEvent("n1", 1, "activity_started", "E0", now - 120000, "trigger=button")) +
            (1..5).map { ActivityEvent("n${it+1}", (it+1).toLong(), "state_changed", "E$it", now - 120000 + it*10000, "previous_state=E${it-1}") } +
            ActivityEvent("n7", 7, "activity_completed", "E5", now - 60000, "")
        return flowOf(values)
    }
}
