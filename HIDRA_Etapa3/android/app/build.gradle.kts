import java.util.Properties
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}
val firebaseConfig = Properties().apply {
    val file = rootProject.file("firebase.local.properties")
    if (file.exists()) file.inputStream().use { load(it) }
}
fun config(name: String, fallback: String = "") =
    "\"" + firebaseConfig.getProperty(name, fallback).replace("\\", "\\\\").replace("\"", "\\\"") + "\""
android {
    namespace = "br.usp.hidra"
    compileSdk = 35
    defaultConfig {
        applicationId = "br.usp.hidra"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        buildConfigField("String", "FIREBASE_API_KEY", config("apiKey"))
        buildConfigField("String", "FIREBASE_APP_ID", config("applicationId", "1:1234567890:android:1234567890abcdef"))
        buildConfigField("String", "FIREBASE_PROJECT_ID", config("projectId", "demo-hidra"))
        buildConfigField("String", "FIREBASE_DATABASE_URL", config("databaseUrl", "https://demo-hidra-default-rtdb.firebaseio.com"))
        buildConfigField("String", "EMULATOR_HOST", config("emulatorHost", "10.0.2.2"))
    }
    flavorDimensions += "backend"
    productFlavors {
        create("demo") { dimension = "backend"; applicationIdSuffix = ".demo" }
        create("emulator") { dimension = "backend"; applicationIdSuffix = ".emulator" }
        create("cloud") { dimension = "backend" }
    }
    buildFeatures { compose = true; buildConfig = true }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}
dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:1.9.0")
    implementation(platform("com.google.firebase:firebase-bom:33.10.0"))
    implementation("com.google.firebase:firebase-auth")
    implementation("com.google.firebase:firebase-database")
    debugImplementation("androidx.compose.ui:ui-tooling")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
}
