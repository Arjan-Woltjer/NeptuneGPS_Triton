import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Release signing (NeptuneGPS_Triton#63). The upload key lives outside the
// repository; a properties file names it:
//   storeFile=<absolute or project-relative path to the .jks>
//   storePassword=...
//   keyAlias=...
//   keyPassword=...
// Local builds point SPRAYCOMPUTER_KEYSTORE_PROPERTIES at that file (or drop it in
// this directory as keystore.properties, which is gitignored); CI writes one
// from its secrets. Without it the release build type has no signing config
// and `bundleRelease` produces an unsigned bundle, which is fine for checking
// that R8 keeps the app working.
val keystoreProperties: Properties? = run {
    val path = System.getenv("SPRAYCOMPUTER_KEYSTORE_PROPERTIES") ?: "keystore.properties"
    val file = rootProject.file(path)
    if (file.isFile) Properties().apply { file.inputStream().use { load(it) } } else null
}

// Version identity comes from the environment so a tag build stamps itself:
// the release workflow sets both from the tag and the run number. Local
// builds fall back to a development marker; the value must only ever go up
// on Play, and the run number does.
val versionCodeFromEnv = System.getenv("SPRAYCOMPUTER_VERSION_CODE")?.toIntOrNull() ?: 1
val versionNameFromEnv = System.getenv("SPRAYCOMPUTER_VERSION_NAME") ?: "1.0.0-dev"
val gitSha = System.getenv("GITHUB_SHA")?.take(7) ?: runCatching {
    val process = ProcessBuilder("git", "rev-parse", "--short=7", "HEAD")
        .directory(rootDir).redirectErrorStream(true).start()
    process.inputStream.bufferedReader().readText().trim().takeIf { process.waitFor() == 0 }
}.getOrNull() ?: "unknown"

android {
    namespace = "nl.meijworks.spraycomputerld"
    // Play requires new apps to target API 36 (Android 16) since 2026-08-31.
    compileSdk = 36

    defaultConfig {
        applicationId = "nl.meijworks.spraycomputerld"
        minSdk = 26
        targetSdk = 36
        versionCode = versionCodeFromEnv
        versionName = versionNameFromEnv
        buildConfigField("String", "GIT_SHA", "\"$gitSha\"")
    }

    // A committed debug keystore (standard Android debug credentials, not a
    // secret) so every CI build signs identically: without it each runner
    // generates its own key and a newer APK refuses to install over an
    // older one (INSTALL_FAILED_UPDATE_INCOMPATIBLE) until uninstalled.
    signingConfigs {
        getByName("debug") {
            storeFile = rootProject.file("debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
        keystoreProperties?.let { props ->
            create("release") {
                val store = props.getProperty("storeFile")
                storeFile = if (File(store).isAbsolute) File(store) else rootProject.file(store)
                storePassword = props.getProperty("storePassword")
                keyAlias = props.getProperty("keyAlias")
                keyPassword = props.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (keystoreProperties != null) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
        }
    }
    buildFeatures {
        compose = true
        buildConfig = true
    }
    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
    }
}

// StringResourcesTest reads src/main/res straight off disk, because what it
// checks is the files themselves. Gradle does not otherwise count them as a
// test input -- editing a translation changes no class and no resource id --
// so the task stayed UP-TO-DATE and the guard never ran on exactly the change
// it exists to catch. Verified: without this, reintroducing "5 %%" and
// deleting a German key left the build green (NeptuneGPS_Triton#142).
tasks.withType<Test>().configureEach {
    inputs.dir("src/main/res")
        .withPropertyName("stringResources")
        .withPathSensitivity(PathSensitivity.RELATIVE)
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    debugImplementation("androidx.compose.ui:ui-tooling")

    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.6")
    implementation("androidx.lifecycle:lifecycle-service:2.8.6")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")

    // The protocol parser is plain Kotlin; its tests run on the JVM in CI.
    testImplementation("junit:junit:4.13.2")
}
