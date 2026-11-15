package br.usp.hidra

import android.os.Bundle
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.google.firebase.FirebaseApp
import com.google.firebase.FirebaseOptions
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.FirebaseDatabase
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

class MainActivity: ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val demo = BuildConfig.FLAVOR == "demo"
        if(BuildConfig.FLAVOR == "cloud" && (BuildConfig.FIREBASE_API_KEY.isBlank() || BuildConfig.FIREBASE_PROJECT_ID == "demo-hidra")) {
            setContent { MaterialTheme { Surface(Modifier.fillMaxSize()) {
                Text("Aplicativo ainda não configurado. Solicite uma versão configurada ao responsável técnico.", Modifier.safeDrawingPadding().padding(24.dp))
            } } }
            return
        }
        val auth: AuthRepository
        val repo: HidraRepository
        if(demo) {
            auth=DemoAuthRepository(); repo=FakeHidraRepository()
        } else {
            val existing = FirebaseApp.getApps(this).firstOrNull()
            val app = existing ?: FirebaseApp.initializeApp(this, FirebaseOptions.Builder()
                .setApiKey(BuildConfig.FIREBASE_API_KEY.ifBlank { "demo-api-key" })
                .setApplicationId(BuildConfig.FIREBASE_APP_ID).setProjectId(BuildConfig.FIREBASE_PROJECT_ID)
                .setDatabaseUrl(BuildConfig.FIREBASE_DATABASE_URL).build())
            val firebaseAuth = FirebaseAuth.getInstance(app)
            val db = FirebaseDatabase.getInstance(app)
            if(BuildConfig.FLAVOR == "emulator" && existing == null) {
                firebaseAuth.useEmulator(BuildConfig.EMULATOR_HOST, 9099)
                db.useEmulator(BuildConfig.EMULATOR_HOST, 9000)
            }
            auth=FirebaseAuthRepository(firebaseAuth); repo=FirebaseHidraRepository(db, firebaseAuth)
        }
        setContent {
            MaterialTheme {
                val vm: HidraViewModel = viewModel(factory=HidraViewModel.factory(auth, repo))
                val owner = LocalLifecycleOwner.current
                DisposableEffect(owner, vm) {
                    val observer = LifecycleEventObserver { _, event ->
                        if(event == Lifecycle.Event.ON_STOP) vm.pause()
                        if(event == Lifecycle.Event.ON_START) vm.resume()
                    }
                    owner.lifecycle.addObserver(observer)
                    onDispose { owner.lifecycle.removeObserver(observer); vm.pause() }
                }
                val state by vm.state.collectAsStateWithLifecycle()
                val cm = getSystemService(ConnectivityManager::class.java)
                val caps = cm.getNetworkCapabilities(cm.activeNetwork)
                val online = caps?.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED) == true
                HidraApp(state, vm, demo, online)
            }
        }
    }
}
private fun date(ms: Long?): String = ms?.let {
    DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm:ss").withZone(ZoneId.systemDefault()).format(Instant.ofEpochMilli(it))
} ?: "Horário indisponível"

@Composable private fun Info(title: String, text: String) {
    Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(16.dp), verticalArrangement=Arrangement.spacedBy(6.dp)) {
        Text(title, style=MaterialTheme.typography.titleMedium); Text(text)
    } }
}
@Composable fun HidraApp(s: UiState, vm: HidraViewModel, demo: Boolean, online: Boolean) {
    BackHandler(s.screen == Screen.HISTORY || s.screen == Screen.DETAILS) {
        if(s.screen == Screen.DETAILS) vm.history() else vm.dashboard()
    }
    Surface(Modifier.fillMaxSize()) {
        Column(Modifier.safeDrawingPadding().padding(20.dp).verticalScroll(rememberScrollState()),
            verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("HIDRA", style=MaterialTheme.typography.headlineLarge)
            Text("Acompanhamento da atividade", style=MaterialTheme.typography.titleMedium)
            if(demo) Info("Demonstração", "Dados fictícios para avaliação da interface.")
            if(!online && !demo) Info("Telefone sem conexão", "Os dados apresentados podem estar desatualizados.")
            if(s.loading) LinearProgressIndicator(Modifier.fillMaxWidth())
            s.error?.let { Text(it, color=MaterialTheme.colorScheme.error); TextButton(onClick=vm::retry) { Text("Tentar novamente") } }
            when(s.screen) {
                Screen.LOGIN -> {
                    var email by remember { mutableStateOf("") }
                    var password by remember { mutableStateOf("") }
                    if(!demo) {
                        OutlinedTextField(email, {email=it}, label={Text("E-mail")}, singleLine=true, modifier=Modifier.fillMaxWidth())
                        OutlinedTextField(password, {password=it}, label={Text("Senha")}, singleLine=true,
                            visualTransformation=PasswordVisualTransformation(), modifier=Modifier.fillMaxWidth())
                    }
                    Button(onClick={vm.login(email, password); password=""}, enabled=!s.loading && (demo || (email.isNotBlank() && password.isNotBlank()))) {
                        Text(if(demo) "Abrir demonstração" else "Entrar")
                    }
                }
                Screen.DEVICES -> {
                    Text("Dispositivos", style=MaterialTheme.typography.headlineSmall)
                    if(!s.loading && s.devices.isEmpty()) Text("Esta conta não possui dispositivo vinculado. Solicite o vínculo ao responsável técnico.")
                    s.devices.forEach { device -> Button(onClick={vm.selectDevice(device)}) { Text(device.label) } }
                }
                Screen.DASHBOARD -> {
                    Text(s.device?.label.orEmpty(), style=MaterialTheme.typography.headlineSmall)
                    val status = s.status
                    if(status == null && !s.loading) Text("Nenhum estado publicado pelo dispositivo.")
                    status?.let {
                        val stale = it.lastSeenMs == null || s.now - it.lastSeenMs > 90000
                        Info(if(stale) "Comunicação do dispositivo desatualizada" else "Último estado observado: ${it.state}",
                            "Última comunicação: ${date(it.lastSeenMs)}\nObservação: ${date(it.observedAtMs)}\nSessão: ${it.sessionId ?: "nenhuma"}")
                        if(it.pendingCount > 0) Text("${it.pendingCount} evento(s) pendente(s). O histórico pode estar atrasado.")
                        if(it.droppedCount > 0) Text("${it.droppedCount} perda(s) registrada(s). Histórico incompleto.")
                    }
                    s.currentSession?.let { session ->
                        Text(resultLabel(session.result))
                        if(session.activeAlerts.isEmpty()) Text("Sem alertas ativos informados nesta sessão.")
                        session.activeAlerts.forEach { Info("Auxílio solicitado", "${it.reason}\n${date(it.occurredAtMs)}") }
                    }
                    Button(onClick={vm.history()}) { Text("Consultar histórico") }
                    Text("Alertas são consultados com o aplicativo aberto.", style=MaterialTheme.typography.bodySmall)
                }
                Screen.HISTORY -> {
                    Text("Histórico", style=MaterialTheme.typography.headlineSmall)
                    if(s.sessions.isEmpty() && !s.loading) Text("Nenhuma sessão registrada.")
                    s.sessions.forEach { session ->
                        OutlinedButton(onClick={vm.details(session)}, modifier=Modifier.fillMaxWidth()) {
                            Text("${date(session.startedAtMs)}\n${resultLabel(session.result)}\n${session.alertCount} alerta(s)")
                        }
                    }
                    if(s.canLoadMore) Button(onClick={vm.history(true)}, enabled=!s.loading) { Text("Carregar anteriores") }
                    TextButton(onClick=vm::dashboard) { Text("Voltar ao acompanhamento") }
                }
                Screen.DETAILS -> {
                    Text("Detalhes da sessão", style=MaterialTheme.typography.headlineSmall)
                    s.selectedSession?.let { session ->
                        Info(resultLabel(session.result), "Início: ${date(session.startedAtMs)}\nFim: ${date(session.endedAtMs)}\n${session.instructionCount} instrução(ões); ${session.alertCount} alerta(s)")
                        if(session.historyIncomplete) Text("Histórico incompleto: há eventos ausentes ou isolados para diagnóstico.")
                        session.estimatedIntakeMl?.let { Text("Volume estimado pela variação de massa: $it mL") }
                    }
                    if(s.events.isEmpty() && !s.loading) Text("Nenhum evento disponível.")
                    s.events.forEach { Info("${it.sequence}. ${it.description}", "${date(it.occurredAtMs)}\n${it.type} • ${it.state}\n${it.details}") }
                    Text("Os registros não confirmam ingestão de água.", style=MaterialTheme.typography.bodySmall)
                    TextButton(onClick={vm.history()}) { Text("Voltar ao histórico") }
                }
            }
            if(s.screen != Screen.LOGIN) TextButton(onClick=vm::logout) { Text("Sair") }
        }
    }
}
