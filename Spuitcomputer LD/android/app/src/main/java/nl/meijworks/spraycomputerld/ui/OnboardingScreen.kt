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

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.BatteryFull
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R

/**
 * Shown once before the operator ever sees the status screen, and again from
 * Settings on request (NeptuneGPS_Triton#123).
 *
 * Android's own permission dialogs say nothing about *why* an app wants
 * Bluetooth, and a second refusal is final: the system stops asking and the
 * app is left inert with no explanation. So explain first, ask second. Play
 * reviewers look for exactly this for a `connectedDevice` foreground service.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun OnboardingScreen(
    permissionsGranted: Boolean,
    batteryExempt: Boolean,
    onGrantPermissions: () -> Unit,
    onBatteryOptimizations: () -> Unit,
    onContinue: () -> Unit,
    onSkip: () -> Unit,
) {
    Scaffold(
        topBar = { TopAppBar(title = { Text(stringResource(R.string.onboarding_title)) }) }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(stringResource(R.string.onboarding_intro), style = MaterialTheme.typography.bodyMedium)

            Reason(
                icon = Icons.Filled.Bluetooth,
                title = stringResource(R.string.onboarding_bluetooth_title),
                body = stringResource(R.string.onboarding_bluetooth_body),
                satisfied = permissionsGranted,
            )
            Reason(
                icon = Icons.Filled.Notifications,
                title = stringResource(R.string.onboarding_notifications_title),
                body = stringResource(R.string.onboarding_notifications_body),
                satisfied = permissionsGranted,
            )
            Reason(
                icon = Icons.Filled.BatteryFull,
                title = stringResource(R.string.onboarding_battery_title),
                body = stringResource(R.string.onboarding_battery_body),
                satisfied = batteryExempt,
            )

            if (!permissionsGranted) {
                Button(onClick = onGrantPermissions, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.onboarding_grant))
                }
            }
            if (!batteryExempt) {
                OutlinedButton(onClick = onBatteryOptimizations, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.onboarding_battery_exempt))
                }
            }

            Button(
                onClick = onContinue,
                modifier = Modifier.fillMaxWidth(),
                enabled = permissionsGranted,
            ) { Text(stringResource(R.string.onboarding_continue)) }

            // The battery exemption is genuinely optional and the permissions
            // can be granted later from the status screen, so never trap the
            // operator on this screen.
            TextButton(onClick = onSkip, modifier = Modifier.fillMaxWidth()) {
                Text(stringResource(R.string.onboarding_skip))
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun Reason(icon: ImageVector, title: String, body: String, satisfied: Boolean) {
    Card(Modifier.fillMaxWidth()) {
        Row(Modifier.padding(16.dp), verticalAlignment = Alignment.Top) {
            Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
                Text(body, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (satisfied) {
                Spacer(Modifier.width(8.dp))
                Icon(
                    Icons.Filled.CheckCircle,
                    contentDescription = stringResource(R.string.onboarding_granted),
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(20.dp),
                )
            }
        }
    }
}
