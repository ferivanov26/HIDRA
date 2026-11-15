package br.usp.hidra

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch

enum class Screen { LOGIN, DEVICES, DASHBOARD, HISTORY, DETAILS }
data class UiState(
    val screen: Screen = Screen.LOGIN, val loading: Boolean = false, val error: String? = null,
    val devices: List<Device> = emptyList(), val device: Device? = null,
    val status: DeviceStatus? = null, val currentSession: SessionSummary? = null,
    val sessions: List<SessionSummary> = emptyList(), val selectedSession: SessionSummary? = null,
    val events: List<ActivityEvent> = emptyList(), val canLoadMore: Boolean = false,
    val now: Long = System.currentTimeMillis()
)
class HidraViewModel(private val auth: AuthRepository, private val repo: HidraRepository): ViewModel() {
    private val mutable = MutableStateFlow(UiState())
    val state = mutable.asStateFlow()
    private var statusJob: Job? = null
    private var currentJob: Job? = null
    private var pageJob: Job? = null
    private var detailJob: Job? = null
    private var loginJob: Job? = null
    private var devicesJob: Job? = null
    init {
        viewModelScope.launch { while(true) { delay(1000); mutable.update { it.copy(now = System.currentTimeMillis()) } } }
        if (auth.signedIn) loadDevices()
    }
    private fun fail() { mutable.update { it.copy(loading=false, error="Não foi possível consultar os dados. Verifique a conexão e o vínculo da conta.") } }
    fun login(email: String, password: String) {
        loginJob?.cancel()
        loginJob = viewModelScope.launch {
            mutable.update { it.copy(loading=true, error=null) }
            try { auth.signIn(email.trim(), password); loadDevices() }
            catch(e: kotlinx.coroutines.CancellationException) { throw e }
            catch(e: Exception) { mutable.update { it.copy(loading=false, error="Falha no acesso. Verifique e-mail, senha e conexão.") } }
        }
    }
    private fun loadDevices() {
        devicesJob?.cancel()
        devicesJob = viewModelScope.launch {
            mutable.update { it.copy(loading=true, error=null) }
            try {
                val devices = repo.devices()
                mutable.update { it.copy(screen=Screen.DEVICES, devices=devices, loading=false) }
                if(devices.size == 1) selectDevice(devices.single())
            } catch(e: kotlinx.coroutines.CancellationException) { throw e }
            catch(e: Exception) { fail() }
        }
    }
    fun selectDevice(device: Device) {
        cancelObservers()
        mutable.update { UiState(screen=Screen.DASHBOARD, device=device, devices=it.devices, loading=true) }
        statusJob = viewModelScope.launch {
            repo.status(device.id).catch { fail() }.collect { status ->
                val previous = mutable.value.status?.sessionId
                mutable.update { it.copy(status=status, loading=false, error=null) }
                if(previous != status?.sessionId) {
                    currentJob?.cancel()
                    mutable.update { it.copy(currentSession=null) }
                    status?.sessionId?.let { session ->
                        currentJob = viewModelScope.launch { repo.summary(device.id, session).catch { fail() }
                            .collect { value -> mutable.update { it.copy(currentSession=value) } } }
                    }
                }
            }
        }
    }
    fun history(older: Boolean = false) {
        val device = mutable.value.device ?: return
        statusJob?.cancel(); currentJob?.cancel()
        val before = if(older) mutable.value.sessions.lastOrNull()?.order else null
        pageJob?.cancel(); detailJob?.cancel()
        mutable.update { it.copy(screen=Screen.HISTORY, loading=true, error=null, selectedSession=null, events=emptyList()) }
        val previous = if(older) mutable.value.sessions else emptyList()
        pageJob = viewModelScope.launch {
            repo.history(device.id, before).catch { fail() }.collect { values ->
                mutable.update { it.copy(sessions=(previous + values).distinctBy { s -> s.id },
                    loading=false, canLoadMore=values.size == 20, error=null) }
            }
        }
    }
    fun details(session: SessionSummary) {
        val device = mutable.value.device ?: return
        statusJob?.cancel(); currentJob?.cancel()
        pageJob?.cancel(); detailJob?.cancel()
        mutable.update { it.copy(screen=Screen.DETAILS, loading=true, selectedSession=session, events=emptyList(), error=null) }
        detailJob = viewModelScope.launch {
            launch { repo.summary(device.id, session.id).catch { fail() }.collect { summary ->
                mutable.update { it.copy(selectedSession=summary) }
            } }
            launch { repo.events(device.id, session.id).catch { fail() }.collect { values ->
                mutable.update { it.copy(events=values, loading=false) }
            } }
        }
    }
    fun dashboard() {
        mutable.value.device?.let(::selectDevice)
    }
    fun retry() {
        when(mutable.value.screen) {
            Screen.LOGIN -> Unit
            Screen.DEVICES -> loadDevices()
            Screen.DASHBOARD -> mutable.value.device?.let(::selectDevice)
            Screen.HISTORY -> history()
            Screen.DETAILS -> mutable.value.selectedSession?.let(::details)
        }
    }
    private fun cancelObservers() { statusJob?.cancel(); currentJob?.cancel(); pageJob?.cancel(); detailJob?.cancel() }
    fun pause() { cancelObservers() }
    fun resume() {
        val saved = mutable.value
        saved.device?.let {
            selectDevice(it)
            when(saved.screen) {
                Screen.HISTORY -> history()
                Screen.DETAILS -> saved.selectedSession?.let(::details)
                else -> Unit
            }
        }
    }
    fun logout() { loginJob?.cancel(); devicesJob?.cancel(); cancelObservers(); auth.signOut(); mutable.value=UiState() }
    override fun onCleared() { cancelObservers() }
    companion object {
        fun factory(auth: AuthRepository, repo: HidraRepository) = object: ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T: ViewModel> create(modelClass: Class<T>): T = HidraViewModel(auth, repo) as T
        }
    }
}
