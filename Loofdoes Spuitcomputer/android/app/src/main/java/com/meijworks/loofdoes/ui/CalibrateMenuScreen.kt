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

/**
 * The Calibrate menu (NeptuneGPS_Triton#46). The four calibration entries
 * arrive with #52; until then they say so. Advanced is live: the tables and
 * settings read from the board, plus a bench console.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CalibrateMenuScreen(sprayer: SprayerState, onBack: () -> Unit, onAdvanced: () -> Unit) {
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
                    MenuEntry("Wizard", "Full calibration: knob positions, then the pump curve", enabled = false)
                    HorizontalDivider()
                    MenuEntry("Potmeter calibration", "The three knob positions only", enabled = false)
                    HorizontalDivider()
                    MenuEntry("Sprayer", "Width and guidance timeout", enabled = false)
                    HorizontalDivider()
                    MenuEntry("GPS config", "Baudrate and minimum fix quality", enabled = false)
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
                "Wizard, potmeter, sprayer and GPS settings arrive in the next version. " +
                    "Until then the serial menu on the board does those.",
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
