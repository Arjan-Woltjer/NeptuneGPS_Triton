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

package nl.meijworks.spraycomputerld.ui

import android.os.SystemClock
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import nl.meijworks.spraycomputerld.ConnectionState
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.textOrEmpty
import nl.meijworks.spraycomputerld.protocol.CalibrationOwner
import nl.meijworks.spraycomputerld.protocol.StatusSample
import kotlinx.coroutines.delay

enum class Screen { ONBOARDING, STATUS, CALIBRATE, WIZARD, POTMETER, SPRAYER, GPS, ADVANCED, CONSOLE, SETTINGS }

/**
 * Home: speed, requested l/ha and actual l/ha large, the rest below, one
 * Calibrate button. Actual turns red while the board reports a deviation.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StatusScreen(
    sprayer: SprayerState,
    serviceRunning: Boolean,
    bluetoothEnabled: Boolean,
    permissionsBlocked: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    onEnableBluetooth: () -> Unit,
    onOpenAppSettings: () -> Unit,
    onCalibrate: () -> Unit,
    onSettings: () -> Unit,
) {
    // Tick once a second so "stale" and fix age update without new lines.
    var now by remember { mutableLongStateOf(SystemClock.elapsedRealtime()) }
    LaunchedEffect(sprayer.connected) {
        while (sprayer.connected) {
            now = SystemClock.elapsedRealtime()
            delay(1000)
        }
    }
    val stale = sprayer.statusStale(now)

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.status_title)) },
                actions = {
                    IconButton(onClick = onSettings) {
                        Icon(Icons.Filled.Settings, contentDescription = stringResource(R.string.action_settings))
                    }
                },
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            // Two things that leave the app inert with no way forward
            // unless the screen says so (NeptuneGPS_Triton#123).
            if (!bluetoothEnabled) {
                ProblemCard(
                    title = stringResource(R.string.status_bluetooth_off_title),
                    body = stringResource(R.string.status_bluetooth_off_body),
                    action = stringResource(R.string.status_bluetooth_turn_on),
                    onAction = onEnableBluetooth,
                )
            }
            if (permissionsBlocked) {
                ProblemCard(
                    title = stringResource(R.string.status_permission_blocked_title),
                    body = stringResource(R.string.status_permission_blocked_body),
                    action = stringResource(R.string.status_open_app_settings),
                    onAction = onOpenAppSettings,
                )
            }
            ConnectionCard(sprayer, serviceRunning, onConnect, onDisconnect)
            DoseTiles(sprayer.status, stale)
            DetailsCard(sprayer, now)
            Button(onClick = onCalibrate, modifier = Modifier.fillMaxWidth(), enabled = sprayer.connected) {
                Icon(Icons.Filled.Tune, contentDescription = null)
                Spacer(Modifier.width(8.dp))
                Text(stringResource(R.string.action_calibrate))
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

/** Something is in the way and the operator can fix it from here. */
@Composable
private fun ProblemCard(title: String, body: String, action: String, onAction: () -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(
                title,
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Text(
                body,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Button(onClick = onAction) { Text(action) }
        }
    }
}

// ------------------------------------------------------------- connection

@Composable
private fun ConnectionCard(
    sprayer: SprayerState,
    serviceRunning: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
) {
    val connected = sprayer.connected
    val bg by animateColorAsState(
        targetValue = when {
            connected -> Color(0xFF16A34A)
            serviceRunning -> Color(0xFFCA8A04)
            else -> MaterialTheme.colorScheme.surfaceVariant
        },
        label = "connectionColor",
    )
    val fg = if (connected || serviceRunning) Color.White else MaterialTheme.colorScheme.onSurfaceVariant

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(bg)
            .padding(16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(
                text = when (sprayer.connection) {
                    ConnectionState.CONNECTED -> stringResource(R.string.connection_connected)
                    ConnectionState.CONNECTING -> stringResource(R.string.connection_connecting)
                    ConnectionState.SCANNING -> stringResource(R.string.connection_scanning)
                    ConnectionState.OFF -> stringResource(R.string.connection_off)
                },
                fontSize = 22.sp,
                fontWeight = FontWeight.Bold,
                color = fg,
            )
            // Hoisted: buildString's lambda is no place for a resource lookup.
            val pairingHint = stringResource(R.string.connection_pairing)
            val firmwareLabel = sprayer.firmwareVersion?.let { stringResource(R.string.connection_firmware, it) }
            val message = sprayer.lastMessage.textOrEmpty()
            val tapConnect = stringResource(R.string.connection_tap_connect)
            val sub = if (sprayer.pairing) pairingHint else buildString {
                sprayer.deviceName?.let { append(it) }
                firmwareLabel?.let { if (isNotEmpty()) append(" · "); append(it) }
                if (isEmpty()) append(message.ifEmpty { tapConnect })
            }
            Text(sub, color = fg.copy(alpha = 0.85f), style = MaterialTheme.typography.bodySmall)
        }
        if (serviceRunning) {
            OutlinedButton(onClick = onDisconnect) { Text(stringResource(R.string.action_disconnect), color = fg) }
        } else {
            Button(onClick = onConnect) { Text(stringResource(R.string.action_connect)) }
        }
    }
}

// ------------------------------------------------------------------ tiles

@Composable
private fun DoseTiles(status: StatusSample?, stale: Boolean) {
    val deviation = status?.deviation == true && !stale
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        val none = stringResource(R.string.value_none)
        val lha = stringResource(R.string.unit_lha)
        Tile(
            label = stringResource(R.string.tile_speed),
            value = status?.let { "%.1f".format(it.speedKmh) } ?: none,
            unit = stringResource(R.string.unit_kmh),
            dim = stale,
        )
        Tile(
            label = stringResource(R.string.tile_requested),
            value = status?.let { "%.0f".format(it.requestedLha) } ?: none,
            unit = lha,
            dim = stale,
        )
        Tile(
            label = stringResource(R.string.tile_actual),
            value = status?.actualLha?.let { "%.0f".format(it) } ?: none,
            unit = lha,
            dim = stale,
            alert = deviation,
        )
    }
    if (deviation) {
        Text(
            stringResource(R.string.status_deviation),
            color = MaterialTheme.colorScheme.error,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center,
        )
    } else if (stale && status != null) {
        Text(
            stringResource(R.string.status_stale),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            style = MaterialTheme.typography.bodySmall,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center,
        )
    }
}

@Composable
private fun RowScope.Tile(label: String, value: String, unit: String, dim: Boolean, alert: Boolean = false) {
    val bg by animateColorAsState(
        targetValue = if (alert) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.surfaceVariant,
        label = "tileColor",
    )
    val fg = if (alert) MaterialTheme.colorScheme.onError else MaterialTheme.colorScheme.onSurfaceVariant
    Column(
        modifier = Modifier
            .weight(1f)
            .clip(RoundedCornerShape(16.dp))
            .background(bg)
            .padding(vertical = 16.dp, horizontal = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = fg.copy(alpha = 0.8f))
        Text(
            value,
            fontSize = 36.sp,
            fontWeight = FontWeight.Black,
            color = if (dim) fg.copy(alpha = 0.4f) else fg,
        )
        Text(unit, style = MaterialTheme.typography.labelSmall, color = fg.copy(alpha = 0.8f))
    }
}

// ---------------------------------------------------------------- details

@Composable
private fun DetailsCard(sprayer: SprayerState, now: Long) {
    val s = sprayer.status
    val g = sprayer.gps
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            val context = LocalContext.current
            val none = stringResource(R.string.value_none)
            Text(stringResource(R.string.details_title), style = MaterialTheme.typography.titleMedium)
            DetailRow(stringResource(R.string.detail_flow), s?.let { stringResource(R.string.value_ml_min, it.flowMlMin) } ?: none)
            DetailRow(stringResource(R.string.detail_knob), s?.let { stringResource(R.string.value_of_4095, it.raw) } ?: none)
            DetailRow(stringResource(R.string.detail_pump_pwm), s?.let { stringResource(R.string.value_of_4095, it.pumpPwm) } ?: none)
            IoMatrix(s)
            if (s != null && s.calibrationOwner != CalibrationOwner.NONE) {
                DetailRow(
                    stringResource(R.string.detail_calibration),
                    if (s.calibrationOwner == CalibrationOwner.APP) stringResource(R.string.calibration_held_by_app)
                    else stringResource(R.string.calibration_held_by_serial),
                )
            }
            sprayer.runSecondsRemaining?.let {
                DetailRow(stringResource(R.string.detail_pump_run), stringResource(R.string.value_seconds_remaining, it))
            }
            HorizontalDivider()
            DetailRow(stringResource(R.string.detail_gps), g?.qualityText?.resolve(context) ?: none)
            DetailRow(
                stringResource(R.string.detail_fix_age),
                when {
                    g == null -> none
                    g.fixAgeMs == null -> stringResource(R.string.detail_no_fix_yet)
                    else -> stringResource(R.string.value_seconds, g.fixAgeMs / 1000.0)
                },
            )
            DetailRow(
                stringResource(R.string.detail_position),
                g?.takeIf { it.hasPosition }?.let { stringResource(R.string.value_position, it.latitude, it.longitude) } ?: none,
            )
            if (!sprayer.connected) {
                Text(
                    sprayer.lastMessage.textOrEmpty(),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

/**
 * Switches (inputs) and outputs side by side, one column per channel, a
 * green dot for on and a red one for off, straight from the board's status
 * line. A protocol 1 board sends no bits; the outputs then fall back to the
 * three states the old line carries and the inputs show as unknown.
 */
@Composable
private fun IoMatrix(s: StatusSample?) {
    val labels = listOf(
        stringResource(R.string.io_channel_mixer),
        stringResource(R.string.io_channel_vernevelaar),
        stringResource(R.string.io_channel_pomp),
        stringResource(R.string.io_channel_aux),
    )
    val inputs = s?.inputs?.takeIf { it.size == 4 }
    val outputs = s?.outputs?.takeIf { it.size == 4 }
        ?: s?.let { listOf(it.mixer, it.vernevelaar, it.pump, it.deviation) }
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxWidth()) {
            Spacer(Modifier.weight(1f))
            labels.forEach { l ->
                Text(
                    l,
                    Modifier.weight(1f),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                )
            }
        }
        IoRow(stringResource(R.string.io_inputs), inputs)
        IoRow(stringResource(R.string.io_outputs), outputs)
    }
}

@Composable
private fun IoRow(label: String, states: List<Boolean>?) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant)
        for (i in 0 until 4) {
            val on = states?.getOrNull(i)
            val colour = when (on) {
                true -> Color(0xFF16A34A)
                false -> Color(0xFFDC2626)
                null -> MaterialTheme.colorScheme.outlineVariant
            }
            Row(Modifier.weight(1f), horizontalArrangement = Arrangement.Center) {
                Spacer(
                    Modifier
                        .size(18.dp)
                        .clip(CircleShape)
                        .background(colour),
                )
            }
        }
    }
}

@Composable
fun DetailRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontWeight = FontWeight.Medium)
    }
}
