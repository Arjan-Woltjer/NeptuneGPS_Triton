/*
  SprayComputer LD - Android companion app for the haulm sprayer computer
  Copyright (C) 2026 J.A. Woltjer.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

package nl.meijworks.spraycomputerld

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import nl.meijworks.spraycomputerld.ble.SprayerBleClient
import nl.meijworks.spraycomputerld.service.SprayerController
import nl.meijworks.spraycomputerld.service.WizardMode
import nl.meijworks.spraycomputerld.service.SprayerService
import nl.meijworks.spraycomputerld.ui.CalibrateMenuScreen
import nl.meijworks.spraycomputerld.ui.ConsoleScreen
import nl.meijworks.spraycomputerld.ui.GpsConfigScreen
import nl.meijworks.spraycomputerld.ui.OnboardingScreen
import nl.meijworks.spraycomputerld.ui.PotmeterScreen
import nl.meijworks.spraycomputerld.ui.PumpScreen
import nl.meijworks.spraycomputerld.ui.SprayerConfigScreen
import nl.meijworks.spraycomputerld.ui.WizardScreen
import nl.meijworks.spraycomputerld.ui.Screen
import nl.meijworks.spraycomputerld.ui.SettingsScreen
import nl.meijworks.spraycomputerld.ui.StatusScreen
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private var pendingStart = false

    /** Android has stopped asking; only the app's settings page can fix it now. */
    private var permissionsBlocked by mutableStateOf(false)

    /** Bluetooth can be switched off from outside the app at any moment. */
    private var bluetoothEnabled by mutableStateOf(true)

    private val bluetoothStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) refreshBluetoothState()
        }
    }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { _ ->
            if (SprayerBleClient.hasPermissions(this)) {
                permissionsBlocked = false
                if (pendingStart) SprayerService.start(this)
            } else {
                // A refusal Android will not re-ask about shows no rationale
                // the next time round: that is the only signal it is final.
                permissionsBlocked = SprayerBleClient.requiredPermissions().none {
                    shouldShowRequestPermissionRationale(it)
                }
                if (!permissionsBlocked) {
                    Toast.makeText(this, R.string.permission_bluetooth_needed, Toast.LENGTH_LONG).show()
                }
            }
            pendingStart = false
        }

    private val enableBluetoothLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { refreshBluetoothState() }

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
            SprayComputerTheme {
                val sprayer by SprayerController.state.collectAsStateWithLifecycle()
                val settings by Settings.state.collectAsStateWithLifecycle()
                // First run lands on the explanation, not on a screen whose
                // only button fires a permission dialog out of nowhere.
                var screen by rememberSaveable {
                    mutableStateOf(if (Settings.state.value.onboardingDone) Screen.STATUS else Screen.ONBOARDING)
                }
                var batteryExempt by remember { mutableStateOf(isIgnoringBatteryOptimizations()) }
                val permissionsGranted = SprayerBleClient.hasPermissions(this) && !permissionsBlocked

                // Both can be changed from outside the app while it is open.
                LaunchedEffect(screen) {
                    batteryExempt = isIgnoringBatteryOptimizations()
                    refreshBluetoothState()
                }
                val serviceRunning = SprayerController.isRunning || sprayer.connection != ConnectionState.OFF
                // The calibration a wizard was started from, so leaving it goes back there.
                var wizardOrigin by rememberSaveable { mutableStateOf(Screen.CALIBRATE) }

                // Where Back goes; the wizard needs its explicit cancel so the board
                // gets calibration back, so Back inside it cancels too.
                BackHandler(enabled = screen != Screen.STATUS && screen != Screen.ONBOARDING) {
                    screen = when (screen) {
                        Screen.WIZARD -> { SprayerController.cancelWizard(); wizardOrigin }
                        Screen.POTMETER, Screen.PUMP, Screen.SPRAYER, Screen.GPS, Screen.CONSOLE -> Screen.CALIBRATE
                        else -> Screen.STATUS
                    }
                }

                when (screen) {
                    Screen.ONBOARDING -> OnboardingScreen(
                        permissionsGranted = permissionsGranted,
                        batteryExempt = batteryExempt,
                        onGrantPermissions = { requestPermissions(startService = false) },
                        onBatteryOptimizations = ::requestIgnoreBatteryOptimizations,
                        onContinue = { Settings.setOnboardingDone(true); screen = Screen.STATUS },
                        onSkip = { Settings.setOnboardingDone(true); screen = Screen.STATUS },
                    )
                    Screen.STATUS -> StatusScreen(
                        sprayer = sprayer,
                        serviceRunning = serviceRunning,
                        bluetoothEnabled = bluetoothEnabled,
                        permissionsBlocked = permissionsBlocked,
                        onConnect = ::connect,
                        onDisconnect = { SprayerService.stop(this) },
                        onEnableBluetooth = ::requestEnableBluetooth,
                        onOpenAppSettings = ::openAppSettings,
                        onCalibrate = { screen = Screen.CALIBRATE },
                        onSettings = { screen = Screen.SETTINGS },
                    )
                    Screen.CALIBRATE -> CalibrateMenuScreen(
                        sprayer = sprayer,
                        developerMode = settings.developerMode,
                        onBack = { screen = Screen.STATUS },
                        onPotmeter = { screen = Screen.POTMETER },
                        onPump = { screen = Screen.PUMP },
                        onSprayer = { screen = Screen.SPRAYER },
                        onGps = { screen = Screen.GPS },
                        onConsole = { screen = Screen.CONSOLE },
                    )
                    Screen.CONSOLE -> ConsoleScreen(sprayer = sprayer, onBack = { screen = Screen.CALIBRATE })
                    Screen.WIZARD -> WizardScreen(
                        sprayer = sprayer,
                        title = when (sprayer.wizard?.mode) {
                            WizardMode.DOSE_ONLY, WizardMode.DOSE_SINGLE -> stringResource(R.string.wizard_title_dose)
                            else -> stringResource(R.string.wizard_title_pump)
                        },
                        onClose = { screen = wizardOrigin },
                    )
                    Screen.POTMETER -> PotmeterScreen(
                        sprayer = sprayer,
                        onBack = { screen = Screen.CALIBRATE },
                        onStartWizard = { wizardOrigin = Screen.POTMETER; screen = Screen.WIZARD },
                    )
                    Screen.PUMP -> PumpScreen(
                        sprayer = sprayer,
                        onBack = { screen = Screen.CALIBRATE },
                        onStartWizard = { wizardOrigin = Screen.PUMP; screen = Screen.WIZARD },
                    )
                    Screen.SPRAYER -> SprayerConfigScreen(sprayer = sprayer, onBack = { screen = Screen.CALIBRATE })
                    Screen.GPS -> GpsConfigScreen(sprayer = sprayer, onBack = { screen = Screen.CALIBRATE })
                    Screen.SETTINGS -> SettingsScreen(
                        settings = settings,
                        serviceRunning = serviceRunning,
                        onBack = { screen = Screen.STATUS },
                        onBatteryOptimizations = ::requestIgnoreBatteryOptimizations,
                        isIgnoringBatteryOptimizations = ::isIgnoringBatteryOptimizations,
                        onShowIntroduction = { screen = Screen.ONBOARDING },
                    )
                }
            }
        }
    }

    private fun connect() = requestPermissions(startService = true)

    private fun requestPermissions(startService: Boolean) {
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
            permissionsBlocked = false
            if (startService) SprayerService.start(this)
        } else {
            pendingStart = startService
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    // ---------------------------------------------------------- Bluetooth

    private fun refreshBluetoothState() {
        val adapter = (getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
        // No adapter at all: not something the operator can turn on, so do not
        // nag about it. `uses-feature` already keeps the app off such a device.
        bluetoothEnabled = adapter == null || adapter.isEnabled
    }

    private fun requestEnableBluetooth() {
        // ACTION_REQUEST_ENABLE throws without BLUETOOTH_CONNECT on 12+, so
        // settle the permission first and let the operator tap again.
        if (!SprayerBleClient.hasPermissions(this)) {
            requestPermissions(startService = false)
            return
        }
        try {
            enableBluetoothLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
        } catch (e: Exception) {
            startActivity(Intent(AndroidSettings.ACTION_BLUETOOTH_SETTINGS))
        }
    }

    private fun openAppSettings() {
        startActivity(
            Intent(AndroidSettings.ACTION_APPLICATION_DETAILS_SETTINGS)
                .setData(Uri.parse("package:$packageName"))
        )
    }

    override fun onStart() {
        super.onStart()
        refreshBluetoothState()
        ContextCompat.registerReceiver(
            this,
            bluetoothStateReceiver,
            IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED),
            ContextCompat.RECEIVER_NOT_EXPORTED,
        )
    }

    override fun onStop() {
        super.onStop()
        runCatching { unregisterReceiver(bluetoothStateReceiver) }
    }

    override fun onResume() {
        super.onResume()
        // Coming back from the system settings page: re-check rather than
        // leave a stale "refused" card on screen.
        refreshBluetoothState()
        if (permissionsBlocked && SprayerBleClient.hasPermissions(this)) permissionsBlocked = false
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
fun SprayComputerTheme(content: @Composable () -> Unit) {
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
