package com.meijworks.spraycomputerld.ui

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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.meijworks.spraycomputerld.ConnectionState
import com.meijworks.spraycomputerld.SprayerState
import com.meijworks.spraycomputerld.protocol.CalibrationOwner
import com.meijworks.spraycomputerld.protocol.StatusSample
import kotlinx.coroutines.delay

enum class Screen { STATUS, CALIBRATE, WIZARD, POTMETER, SPRAYER, GPS, ADVANCED, CONSOLE, SETTINGS }

/**
 * Home: speed, requested l/ha and actual l/ha large, the rest below, one
 * Calibrate button. Actual turns red while the board reports a deviation.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StatusScreen(
    sprayer: SprayerState,
    serviceRunning: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
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
                title = { Text("SprayComputer LD") },
                actions = {
                    IconButton(onClick = onSettings) {
                        Icon(Icons.Filled.Settings, contentDescription = "Settings")
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
            ConnectionCard(sprayer, serviceRunning, onConnect, onDisconnect)
            DoseTiles(sprayer.status, stale)
            DetailsCard(sprayer, now)
            Button(onClick = onCalibrate, modifier = Modifier.fillMaxWidth(), enabled = sprayer.connected) {
                Icon(Icons.Filled.Tune, contentDescription = null)
                Spacer(Modifier.width(8.dp))
                Text("Calibrate")
            }
            Spacer(Modifier.height(24.dp))
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
                    ConnectionState.CONNECTED -> "Connected"
                    ConnectionState.CONNECTING -> "Connecting"
                    ConnectionState.SCANNING -> "Searching"
                    ConnectionState.OFF -> "Not connected"
                },
                fontSize = 22.sp,
                fontWeight = FontWeight.Bold,
                color = fg,
            )
            val sub = if (sprayer.pairing) "Pairing: open the Bluetooth notification and enter the code from the sprayer's display" else buildString {
                sprayer.deviceName?.let { append(it) }
                sprayer.firmwareVersion?.let { if (isNotEmpty()) append(" · "); append("firmware $it") }
                if (isEmpty()) append(sprayer.lastMessage.ifEmpty { "Tap Connect to find the sprayer" })
            }
            Text(sub, color = fg.copy(alpha = 0.85f), style = MaterialTheme.typography.bodySmall)
        }
        if (serviceRunning) {
            OutlinedButton(onClick = onDisconnect) { Text("Disconnect", color = fg) }
        } else {
            Button(onClick = onConnect) { Text("Connect") }
        }
    }
}

// ------------------------------------------------------------------ tiles

@Composable
private fun DoseTiles(status: StatusSample?, stale: Boolean) {
    val deviation = status?.deviation == true && !stale
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Tile(
            label = "Speed",
            value = status?.let { "%.1f".format(it.speedKmh) } ?: "–",
            unit = "km/h",
            dim = stale,
        )
        Tile(
            label = "Requested",
            value = status?.let { "%.0f".format(it.requestedLha) } ?: "–",
            unit = "l/ha",
            dim = stale,
        )
        Tile(
            label = "Actual",
            value = status?.actualLha?.let { "%.0f".format(it) } ?: "–",
            unit = "l/ha",
            dim = stale,
            alert = deviation,
        )
    }
    if (deviation) {
        Text(
            "Dose outside 5 % of requested: adjust speed",
            color = MaterialTheme.colorScheme.error,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center,
        )
    } else if (stale && status != null) {
        Text(
            "No status from the board for a moment…",
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
            Text("Details", style = MaterialTheme.typography.titleMedium)
            DetailRow("Flow", s?.let { "%.0f ml/min".format(it.flowMlMin) } ?: "–")
            DetailRow("Knob", s?.let { "${it.raw} / 4095" } ?: "–")
            DetailRow("Pump PWM", s?.let { "${it.pumpPwm} / 4095" } ?: "–")
            IoMatrix(s)
            if (s != null && s.calibrationOwner != CalibrationOwner.NONE) {
                DetailRow(
                    "Calibration",
                    if (s.calibrationOwner == CalibrationOwner.APP) "held by this app" else "held by the serial wizard",
                )
            }
            sprayer.runSecondsRemaining?.let { DetailRow("Pump run", "$it s remaining") }
            HorizontalDivider()
            DetailRow("GPS", g?.qualityLabel ?: "–")
            DetailRow(
                "Fix age",
                when {
                    g == null -> "–"
                    g.fixAgeMs == null -> "no fix yet"
                    else -> "%.1f s".format(g.fixAgeMs / 1000.0)
                },
            )
            DetailRow("Position", g?.takeIf { it.hasPosition }?.let { "%.6f, %.6f".format(it.latitude, it.longitude) } ?: "–")
            if (!sprayer.connected) {
                Text(
                    sprayer.lastMessage,
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
    val labels = listOf("Mixer", "Vernevelaar", "Pomp", "Aux")
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
        IoRow("Inputs", inputs)
        IoRow("Outputs", outputs)
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
