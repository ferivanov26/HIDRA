package br.usp.hidra

import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.test.*
import org.junit.*
import org.junit.Assert.*

@OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
class HidraViewModelTest {
    private val dispatcher = StandardTestDispatcher()
    private lateinit var vm: HidraViewModel
    @Before fun setup() {
        Dispatchers.setMain(dispatcher)
        vm=HidraViewModel(DemoAuthRepository(), FakeHidraRepository())
    }
    @After fun finish() { vm.viewModelScope.cancel(); Dispatchers.resetMain() }
    @Test fun singleDeviceOpensDashboardAndShowsCurrentAlert() = runTest(dispatcher) {
        vm.login("", ""); runCurrent()
        assertEquals(Screen.DASHBOARD, vm.state.value.screen)
        assertEquals("demo_s2", vm.state.value.status?.sessionId)
        assertEquals(1, vm.state.value.currentSession?.activeAlerts?.size)
        vm.viewModelScope.cancel()
    }
    @Test fun oldSessionDoesNotInheritCurrentAlert() = runTest(dispatcher) {
        vm.login("", ""); runCurrent()
        vm.history(); runCurrent()
        vm.details(vm.state.value.sessions.last()); runCurrent()
        assertEquals(Screen.DETAILS, vm.state.value.screen)
        assertEquals("demo_s1", vm.state.value.selectedSession?.id)
        assertTrue(vm.state.value.selectedSession!!.activeAlerts.isEmpty())
        assertEquals(7, vm.state.value.events.size)
        assertEquals("activity_completed", vm.state.value.events.last().type)
        vm.viewModelScope.cancel()
    }
    @Test fun logoutClearsDeviceAndSessionData() = runTest(dispatcher) {
        vm.login("", ""); runCurrent(); vm.history(); runCurrent()
        vm.logout(); runCurrent()
        assertEquals(Screen.LOGIN, vm.state.value.screen)
        assertNull(vm.state.value.device)
        assertNull(vm.state.value.currentSession)
        assertTrue(vm.state.value.sessions.isEmpty())
        vm.viewModelScope.cancel()
    }
    @Test fun returningFromDetailsRestartsDashboardSubscription() = runTest(dispatcher) {
        vm.login("", ""); runCurrent(); vm.history(); runCurrent()
        vm.details(vm.state.value.sessions.last()); runCurrent()
        vm.dashboard(); runCurrent()
        assertEquals(Screen.DASHBOARD, vm.state.value.screen)
        assertEquals("demo_s2", vm.state.value.currentSession?.id)
        assertTrue(vm.state.value.events.isEmpty())
        vm.viewModelScope.cancel()
    }
}
