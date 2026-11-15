package br.usp.hidra

import android.os.Bundle
import kotlinx.coroutines.channels.awaitClose
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
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

class MainActivity: ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val demo = BuildConfig.FLAVOR == "demo"
        if(BuildConfig.FLAVOR == "cloud" && (BuildConfig.FIREBASE_API_KEY.isBlank() || BuildConfig.FIREBASE_PROJECT_ID == "demo-hidra")) {
            setContent { HidraTheme { Surface(Modifier.fillMaxSize()) {
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
            HidraTheme {
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
                val connected = remember(cm) { connectivity(cm) }
                val online by connected.collectAsStateWithLifecycle(initialValue=hasInternet(cm))
                HidraApp(state, vm, demo, online)
            }
        }
    }
}

private fun hasInternet(cm: ConnectivityManager): Boolean =
    cm.getNetworkCapabilities(cm.activeNetwork)?.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED) == true

private fun connectivity(cm: ConnectivityManager) = kotlinx.coroutines.flow.callbackFlow {
    val callback=object: ConnectivityManager.NetworkCallback() {
        override fun onAvailable(network: android.net.Network) { trySend(hasInternet(cm)) }
        override fun onCapabilitiesChanged(network: android.net.Network, caps: NetworkCapabilities) { trySend(hasInternet(cm)) }
        override fun onLost(network: android.net.Network) { trySend(hasInternet(cm)) }
    }
    cm.registerDefaultNetworkCallback(callback)
    trySend(hasInternet(cm))
    awaitClose { cm.unregisterNetworkCallback(callback) }
}
