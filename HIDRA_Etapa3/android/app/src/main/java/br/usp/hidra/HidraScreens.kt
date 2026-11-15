package br.usp.hidra

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp

@Composable private fun Panel(title: String, text: String, warning: Boolean=false) {
    val colors=MaterialTheme.colorScheme
    Card(Modifier.fillMaxWidth(), shape=RoundedCornerShape(20.dp), colors=CardDefaults.cardColors(
        containerColor=if(warning) colors.errorContainer else colors.surface)) {
        Column(Modifier.padding(20.dp), verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Text(title, style=MaterialTheme.typography.titleMedium,
                color=if(warning) colors.onErrorContainer else colors.onSurface)
            Text(text, style=MaterialTheme.typography.bodyMedium,
                color=if(warning) colors.onErrorContainer else colors.onSurfaceVariant)
        }
    }
}
@Composable private fun Badge(text: String, alert: Boolean=false) {
    Surface(color=if(alert) MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.primaryContainer,
        contentColor=if(alert) MaterialTheme.colorScheme.onErrorContainer else MaterialTheme.colorScheme.onPrimaryContainer,
        shape=RoundedCornerShape(9.dp)) {
        Text(text, Modifier.padding(horizontal=10.dp, vertical=6.dp), style=MaterialTheme.typography.labelMedium)
    }
}
@Composable private fun Note(text: String) {
    Text(text, style=MaterialTheme.typography.bodySmall, color=MaterialTheme.colorScheme.onSurfaceVariant)
}
@Composable private fun SectionTitle(title: String, subtitle: String) {
    Column(verticalArrangement=Arrangement.spacedBy(6.dp)) {
        Text(title, style=MaterialTheme.typography.headlineMedium, fontWeight=FontWeight.SemiBold)
        Text(subtitle, style=MaterialTheme.typography.bodyMedium, color=MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
@Composable fun HidraApp(s: UiState, vm: HidraViewModel, demo: Boolean, online: Boolean) {
    val canBack=s.screen==Screen.HISTORY || s.screen==Screen.DETAILS || (s.screen==Screen.DASHBOARD && s.devices.size>1)
    val back: () -> Unit = {
        when(s.screen) {
            Screen.DETAILS -> vm.history()
            Screen.HISTORY -> vm.dashboard()
            else -> vm.devices()
        }
    }
    BackHandler(canBack, back)
    Surface(Modifier.fillMaxSize(), color=MaterialTheme.colorScheme.background) {
        Column(Modifier.safeDrawingPadding().imePadding().fillMaxSize(), horizontalAlignment=Alignment.CenterHorizontally) {
            Row(Modifier.widthIn(max=640.dp).fillMaxWidth().padding(horizontal=20.dp, vertical=14.dp),
                verticalAlignment=Alignment.CenterVertically, horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                WaterMark()
                Column(Modifier.weight(1f)) {
                    Text("HIDRA", style=MaterialTheme.typography.titleLarge, fontWeight=FontWeight.Bold)
                    Text("Cuidado em cada etapa", style=MaterialTheme.typography.labelMedium,
                        color=MaterialTheme.colorScheme.onSurfaceVariant)
                }
                if(s.screen!=Screen.LOGIN) TextButton(onClick=vm::logout) { Text("Sair") }
            }
            HorizontalDivider(color=MaterialTheme.colorScheme.surfaceVariant)
            key(s.screen, s.device?.id, s.selectedSession?.id) {
                LazyColumn(Modifier.widthIn(max=640.dp).fillMaxWidth().weight(1f),
                    contentPadding=PaddingValues(20.dp), verticalArrangement=Arrangement.spacedBy(16.dp)) {
                    if(canBack) item { TextButton(onClick=back, contentPadding=PaddingValues(0.dp)) {
                        Text(when(s.screen) { Screen.DETAILS -> "‹  Histórico"; Screen.HISTORY -> "‹  Acompanhamento"; else -> "‹  Dispositivos" })
                    } }
                    if(demo) item { Badge("Demonstração · dados fictícios") }
                    if(!online && !demo) item { Panel("Telefone sem internet", "Os dados exibidos podem estar desatualizados. A consulta será retomada quando a conexão voltar.", true) }
                    if(s.loading) item { LinearProgressIndicator(Modifier.fillMaxWidth()) }
                    s.error?.let { message -> item {
                        Panel(if(s.screen==Screen.LOGIN) "Não foi possível entrar" else "Não foi possível carregar", message, true)
                        if(s.screen!=Screen.LOGIN) TextButton(onClick=vm::retry) { Text("Tentar novamente") }
                    } }
                    when(s.screen) {
                        Screen.LOGIN -> item { Login(s, vm, demo) }
                        Screen.DEVICES -> {
                            item { SectionTitle("Seus dispositivos", "Escolha o dispositivo que deseja acompanhar.") }
                            if(s.devices.isEmpty() && !s.loading) item {
                                Panel("Nenhum dispositivo vinculado", "Solicite ao responsável pelo projeto o vínculo do dispositivo à sua conta.")
                            }
                            items(s.devices, key={it.id}) { device ->
                                Card(onClick={vm.selectDevice(device)}, modifier=Modifier.fillMaxWidth(), shape=RoundedCornerShape(20.dp),
                                    colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface)) {
                                    Column(Modifier.padding(20.dp), verticalArrangement=Arrangement.spacedBy(12.dp)) {
                                        WaterMark()
                                        Text(device.label, style=MaterialTheme.typography.titleLarge, fontWeight=FontWeight.SemiBold)
                                        Text("Abrir acompanhamento  →", color=MaterialTheme.colorScheme.primary, style=MaterialTheme.typography.labelLarge)
                                    }
                                }
                            }
                            item { OutlinedButton(onClick=vm::devices, enabled=!s.loading, modifier=Modifier.fillMaxWidth()) { Text("Atualizar dispositivos") } }
                        }
                        Screen.DASHBOARD -> {
                            item { SectionTitle(s.device?.label ?: "Acompanhamento", "Estado do dispositivo e última atividade registrada.") }
                            if(s.status==null && !s.loading) item { Panel("Aguardando o dispositivo", "Nenhum estado foi publicado. Verifique se o dispositivo está ligado e conectado.") }
                            s.status?.let { status ->
                                val stale=status.lastSeenMs==null || s.now-status.lastSeenMs>90000
                                item { StatusCard(status, stale) }
                                if(status.pendingCount>0) item { Panel("Histórico em sincronização", "${countLabel(status.pendingCount, "registro aguarda", "registros aguardam")} envio pelo dispositivo. O histórico pode estar atrasado.") }
                                if(status.droppedCount>0) item { Panel("Há registros ausentes", "O dispositivo informou ${countLabel(status.droppedCount, "perda", "perdas")}. O histórico pode estar incompleto.", true) }
                                if(stale) item { Note("O estado e os alertas abaixo correspondem aos últimos dados recebidos, e podem ter mudado.") }
                            }
                            s.currentSession?.let { session ->
                                item { Text("Última atividade", style=MaterialTheme.typography.titleMedium) }
                                item { SessionCard(session, onClick={vm.details(session)}) }
                                if(session.activeAlerts.isEmpty()) item { Panel("Nenhum alerta ativo informado", "Não há solicitação de auxílio ativa nos últimos dados desta sessão.") }
                                items(session.activeAlerts, key={it.id}) { alert ->
                                    Panel("Auxílio solicitado", reasonLabel(alert.reason) + "\n" +
                                        (alert.occurredAtMs?.let(::formatDate) ?: "Horário da solicitação não registrado."), true)
                                }
                            }
                            item { Button(onClick={vm.history()}, modifier=Modifier.fillMaxWidth(), contentPadding=PaddingValues(16.dp)) { Text("Consultar histórico") } }
                            item { Note("Acompanhe alertas com o aplicativo aberto. Esta versão não envia notificações em segundo plano.") }
                        }
                        Screen.HISTORY -> {
                            item { SectionTitle("Histórico", s.device?.label.orEmpty()) }
                            if(s.sessions.isEmpty() && !s.loading) item { Panel("Nenhuma atividade registrada", "As atividades aparecerão aqui quando o dispositivo enviar seus registros.") }
                            items(s.sessions, key={it.id}) { session -> SessionCard(session, onClick={vm.details(session)}) }
                            if(s.canLoadMore) item { OutlinedButton(onClick={vm.history(true)}, enabled=!s.loading, modifier=Modifier.fillMaxWidth()) { Text("Carregar atividades anteriores") } }
                        }
                        Screen.DETAILS -> {
                            item { SectionTitle("Detalhes da atividade", s.device?.label.orEmpty()) }
                            s.selectedSession?.let { session -> item {
                                Card(shape=RoundedCornerShape(20.dp), colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface)) {
                                    Column(Modifier.fillMaxWidth().padding(20.dp), verticalArrangement=Arrangement.spacedBy(12.dp)) {
                                        Badge(resultLabel(session.result), session.result=="aborted")
                                        Text("Início", style=MaterialTheme.typography.labelMedium)
                                        Text(formatDate(session.startedAtMs))
                                        Text("Encerramento", style=MaterialTheme.typography.labelMedium)
                                        Text(when { session.endedAtMs!=null -> formatDate(session.endedAtMs)
                                            session.result=="in_progress" -> "Atividade em andamento"
                                            else -> "Horário não registrado pelo dispositivo" })
                                        HorizontalDivider(color=MaterialTheme.colorScheme.surfaceVariant)
                                        Text("${countLabel(session.instructionCount, "instrução emitida", "instruções emitidas")}  ·  ${countLabel(session.alertCount, "alerta registrado", "alertas registrados")}", style=MaterialTheme.typography.bodyMedium)
                                        if(session.historyIncomplete) Text("Histórico incompleto: há registros ausentes.", color=MaterialTheme.colorScheme.error)
                                        session.estimatedIntakeMl?.let { Text("Volume estimado pela variação de massa: ${formatAmount(it)} mL") }
                                    }
                                }
                            } }
                            item { Text("Linha do tempo", style=MaterialTheme.typography.titleLarge, fontWeight=FontWeight.SemiBold) }
                            if(s.events.isEmpty() && !s.loading) item { Panel("Nenhum evento disponível", "Os registros desta atividade ainda não foram recebidos.") }
                            items(s.events, key={it.id}) { event -> EventCard(event) }
                            item { Note("A conclusão descreve a sequência observada. Os registros e a estimativa de volume não confirmam ingestão de água.") }
                        }
                    }
                    item { Spacer(Modifier.height(12.dp)) }
                }
            }
        }
    }
}
@Composable private fun Login(s: UiState, vm: HidraViewModel, demo: Boolean) {
    var email by rememberSaveable { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var visible by remember { mutableStateOf(false) }
    val enabled=!s.loading && (demo || (email.isNotBlank() && password.isNotBlank()))
    val submit: () -> Unit = { if(enabled) { vm.login(email,password); password="" } }
    Column(verticalArrangement=Arrangement.spacedBy(20.dp)) {
        Spacer(Modifier.height(16.dp))
        SectionTitle("Acompanhe com tranquilidade", "Consulte as atividades e as solicitações de auxílio dos seus dispositivos HIDRA.")
        Card(shape=RoundedCornerShape(24.dp), colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface)) {
            Column(Modifier.padding(20.dp), verticalArrangement=Arrangement.spacedBy(16.dp)) {
                Text(if(demo) "Conheça o aplicativo" else "Acesso do cuidador", style=MaterialTheme.typography.titleLarge)
                if(!demo) {
                    OutlinedTextField(email, {email=it}, label={Text("E-mail")}, singleLine=true, modifier=Modifier.fillMaxWidth(),
                        keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Email, imeAction=ImeAction.Next))
                    OutlinedTextField(password, {password=it}, label={Text("Senha")}, singleLine=true, modifier=Modifier.fillMaxWidth(),
                        visualTransformation=if(visible) VisualTransformation.None else PasswordVisualTransformation(),
                        trailingIcon={TextButton(onClick={visible=!visible}) { Text(if(visible) "Ocultar" else "Mostrar") }},
                        keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Password, imeAction=ImeAction.Done),
                        keyboardActions=KeyboardActions(onDone={submit()}))
                }
                Button(onClick=submit, enabled=enabled, modifier=Modifier.fillMaxWidth(), contentPadding=PaddingValues(16.dp)) {
                    Text(if(demo) "Explorar demonstração" else "Entrar")
                }
            }
        }
        Note("Utilize a conta cadastrada pelo responsável pelo projeto. O aplicativo permite consultar os dados; as ações da atividade ocorrem no dispositivo.")
    }
}
@Composable private fun StatusCard(status: DeviceStatus, stale: Boolean) {
    Card(Modifier.fillMaxWidth(), shape=RoundedCornerShape(24.dp), colors=CardDefaults.cardColors(
        containerColor=MaterialTheme.colorScheme.primaryContainer)) {
        Column(Modifier.padding(22.dp), verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Badge(if(stale) "Sem comunicação recente" else "Comunicação recente", stale)
            Text(when { stale -> "Aguardando atualização"; status.activityActive==false -> "Sistema em repouso"
                status.activityActive==true -> "Atividade em andamento"; else -> "Último estado recebido" },
                style=MaterialTheme.typography.headlineSmall, fontWeight=FontWeight.SemiBold,
                color=MaterialTheme.colorScheme.onPrimaryContainer)
            Text("${if(status.activityActive==false) "Última etapa" else "Etapa registrada"}: ${stateLabel(status.state)}",
                color=MaterialTheme.colorScheme.onPrimaryContainer)
            HorizontalDivider(color=MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha=.15f))
            Text("Última comunicação\n${formatDate(status.lastSeenMs)}", style=MaterialTheme.typography.bodyMedium,
                color=MaterialTheme.colorScheme.onPrimaryContainer)
            if(status.observedAtMs!=null) Text("Estado observado em ${formatDate(status.observedAtMs)}",
                style=MaterialTheme.typography.bodySmall, color=MaterialTheme.colorScheme.onPrimaryContainer)
            else Text("O horário da observação não foi registrado. A comunicação acima usa o horário do servidor.",
                style=MaterialTheme.typography.bodySmall, color=MaterialTheme.colorScheme.onPrimaryContainer)
        }
    }
}
@Composable private fun SessionCard(session: SessionSummary, onClick: () -> Unit) {
    Card(onClick=onClick, modifier=Modifier.fillMaxWidth(), shape=RoundedCornerShape(20.dp),
        colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface)) {
        Column(Modifier.padding(20.dp), verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Badge(resultLabel(session.result), session.result=="aborted")
            Text(formatDate(session.startedAtMs), style=MaterialTheme.typography.titleMedium)
            Text("${countLabel(session.instructionCount, "instrução", "instruções")}  ·  ${countLabel(session.alertCount, "alerta registrado", "alertas registrados")}",
                style=MaterialTheme.typography.bodyMedium, color=MaterialTheme.colorScheme.onSurfaceVariant)
            if(session.activeAlerts.isNotEmpty()) Text("${countLabel(session.activeAlerts.size.toLong(), "alerta ainda ativo", "alertas ainda ativos")}", color=MaterialTheme.colorScheme.error)
            if(session.historyIncomplete) Text("Histórico incompleto", color=MaterialTheme.colorScheme.error)
            Text("Ver detalhes  →", style=MaterialTheme.typography.labelLarge, color=MaterialTheme.colorScheme.primary)
        }
    }
}
@Composable private fun EventCard(event: ActivityEvent) {
    var technical by rememberSaveable(event.id) { mutableStateOf(false) }
    Card(Modifier.fillMaxWidth(), shape=RoundedCornerShape(20.dp), colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surface)) {
        Column(Modifier.padding(18.dp), verticalArrangement=Arrangement.spacedBy(10.dp)) {
            Row(horizontalArrangement=Arrangement.spacedBy(12.dp), verticalAlignment=Alignment.CenterVertically) {
                Box(Modifier.sizeIn(minWidth=34.dp, minHeight=34.dp).background(MaterialTheme.colorScheme.primaryContainer, CircleShape).padding(8.dp), contentAlignment=Alignment.Center) {
                    Text(event.sequence.toString(), color=MaterialTheme.colorScheme.onPrimaryContainer, style=MaterialTheme.typography.labelLarge)
                }
                Text(event.description, Modifier.weight(1f), style=MaterialTheme.typography.titleMedium)
            }
            Text(event.timeLabel(), style=MaterialTheme.typography.bodySmall, color=MaterialTheme.colorScheme.onSurfaceVariant)
            Text(event.readableDetails(), style=MaterialTheme.typography.bodyMedium)
            TextButton(onClick={technical=!technical}, contentPadding=PaddingValues(0.dp)) { Text(if(technical) "Ocultar dados técnicos" else "Dados técnicos") }
            if(technical) {
                HorizontalDivider()
                Note("${event.type} · ${event.state}\nID: ${event.id}\n${event.details}")
            }
        }
    }
}
