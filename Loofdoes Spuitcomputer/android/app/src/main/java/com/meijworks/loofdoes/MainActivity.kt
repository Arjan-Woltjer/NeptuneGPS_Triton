package com.meijworks.loofdoes

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings as AndroidSettings
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import com.meijworks.loofdoes.ble.SprayerBleClient
import com.meijworks.loofdoes.service.SprayerController
import com.meijworks.loofdoes.service.SprayerService
import com.meijworks.loofdoes.ui.AdvancedScreen
import com.meijworks.loofdoes.ui.CalibrateMenuScreen
import com.meijworks.loofdoes.ui.Screen
import com.meijworks.loofdoes.ui.SettingsScreen
import com.meijworks.loofdoes.ui.StatusScreen
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private var pendingStart = false

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { _ ->
            if (SprayerBleClient.hasPermissions(this)) {
                if (pendingStart) SprayerService.start(this)
            } else {
                Toast.makeText(this, "Bluetooth permission is needed to find the sprayer", Toast.LENGTH_LONG).show()
            }
            pendingStart = false
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Settings.init(this)

        lifecycleScope.launch {
            Settings.state.collect { s ->
                if (s.keepScreenOn) {
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                } else {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                }
            }
        }

        setContent {
            LoofdoesTheme {
                val sprayer by SprayerController.state.collectAsStateWithLifecycle()
                val settings by Settings.state.collectAsStateWithLifecycle()
                var screen by rememberSaveable { mutableStateOf(Screen.STATUS) }
                val serviceRunning = SprayerController.isRunning || sprayer.connection != ConnectionState.OFF

                BackHandler(enabled = screen != Screen.STATUS) {
                    screen = if (screen == Screen.ADVANCED) Screen.CALIBRATE else Screen.STATUS
                }

                when (screen) {
                    Screen.STATUS -> StatusScreen(
                        sprayer = sprayer,
                        serviceRunning = serviceRunning,
                        onConnect = ::connect,
                        onDisconnect = { SprayerService.stop(this) },
                        onCalibrate = { screen = Screen.CALIBRATE },
                        onSettings = { screen = Screen.SETTINGS },
                    )
                    Screen.CALIBRATE -> CalibrateMenuScreen(
                        sprayer = sprayer,
                        onBack = { screen = Screen.STATUS },
                        onAdvanced = { screen = Screen.ADVANCED },
                    )
                    Screen.ADVANCED -> AdvancedScreen(
                        sprayer = sprayer,
                        onBack = { screen = Screen.CALIBRATE },
                    )
                    Screen.SETTINGS -> SettingsScreen(
                        settings = settings,
                        serviceRunning = serviceRunning,
                        onBack = { screen = Screen.STATUS },
                        onBatteryOptimizations = ::requestIgnoreBatteryOptimizations,
                        isIgnoringBatteryOptimizations = ::isIgnoringBatteryOptimizations,
                    )
                }
            }
        }
    }

    private fun connect() {
        val missing = mutableListOf<String>()
        missing += SprayerBleClient.requiredPermissions().filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) !=
            PackageManager.PERMISSION_GRANTED
        ) {
            missing += Manifest.permission.POST_NOTIFICATIONS
        }
        if (missing.isEmpty()) {
            SprayerService.start(this)
        } else {
            pendingStart = true
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun isIgnoringBatteryOptimizations(): Boolean {
        val pm = getSystemService(POWER_SERVICE) as PowerManager
        return pm.isIgnoringBatteryOptimizations(packageName)
    }

    private fun requestIgnoreBatteryOptimizations() {
        try {
            startActivity(
                Intent(AndroidSettings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
                    .setData(Uri.parse("package:$packageName"))
            )
        } catch (e: Exception) {
            startActivity(Intent(AndroidSettings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
        }
    }
}

@Composable
fun LoofdoesTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val scheme = if (dark) {
        darkColorScheme(
            primary = Color(0xFF4ADE80),
            onPrimary = Color(0xFF052E16),
            secondary = Color(0xFF7DD3FC),
            error = Color(0xFFF87171),
        )
    } else {
        lightColorScheme(
            primary = Color(0xFF15803D),
            onPrimary = Color.White,
            secondary = Color(0xFF0369A1),
            error = Color(0xFFDC2626),
        )
    }
    MaterialTheme(colorScheme = scheme, content = content)
}
