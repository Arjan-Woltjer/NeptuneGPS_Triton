package com.meijworks.loofdoes.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.meijworks.loofdoes.SprayerState

/** The Calibrate menu (NeptuneGPS_Triton#46): the five agreed entries. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CalibrateMenuScreen(
    sprayer: SprayerState,
    onBack: () -> Unit,
    onWizard: () -> Unit,
    onPotmeter: () -> Unit,
    onSprayer: () -> Unit,
    onGps: () -> Unit,
    onAdvanced: () -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Calibrate") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier.padding(padding).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Card(Modifier.fillMaxWidth()) {
                Column {
                    MenuEntry("Wizard", "Full calibration: knob positions, then the pump curve", enabled = sprayer.connected, onClick = onWizard)
                    HorizontalDivider()
                    MenuEntry("Potmeter calibration", "The three knob positions only", enabled = sprayer.connected, onClick = onPotmeter)
                    HorizontalDivider()
                    MenuEntry("Sprayer", "Width and guidance timeout", enabled = sprayer.connected, onClick = onSprayer)
                    HorizontalDivider()
                    MenuEntry("GPS config", "Baudrate and minimum fix quality", enabled = sprayer.connected, onClick = onGps)
                    HorizontalDivider()
                    MenuEntry(
                        "Advanced",
                        "Calibration tables, settings and a console",
                        enabled = sprayer.connected,
                        onClick = onAdvanced,
                    )
                }
            }
            Text(
                if (sprayer.connected) "The wizard follows the same steps as the serial menu on the board."
                else "Connect to the sprayer first.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun MenuEntry(title: String, subtitle: String, enabled: Boolean, onClick: () -> Unit = {}) {
    val alpha = if (enabled) 1f else 0.45f
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha))
            Text(
                subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
            )
        }
        Icon(
            Icons.AutoMirrored.Filled.KeyboardArrowRight,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
        )
    }
}
