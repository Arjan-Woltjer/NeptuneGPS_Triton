package nl.meijworks.spraycomputerld.service

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.PowerManager
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import nl.meijworks.spraycomputerld.ConnectionState
import nl.meijworks.spraycomputerld.MainActivity
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.Settings
import nl.meijworks.spraycomputerld.SettingsState
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.UiText
import nl.meijworks.spraycomputerld.uiText
import nl.meijworks.spraycomputerld.audio.AlarmPlayer
import nl.meijworks.spraycomputerld.audio.AlarmSound
import nl.meijworks.spraycomputerld.ble.SprayerBleClient
import nl.meijworks.spraycomputerld.protocol.BoardMessage
import nl.meijworks.spraycomputerld.protocol.DosePoint
import nl.meijworks.spraycomputerld.protocol.PwmPoint
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.util.ArrayDeque

/**
 * Foreground service that owns the BLE link and the alarm. With a partial
 * wake lock the link survives the screen going to sleep, which matters during
 * a 60 s calibration run when the operator walks to the measuring jug, and
 * the deviation alarm keeps sounding while the phone is in a pocket.
 *
 * The board decides everything: this service displays its status lines and
 * turns the board's deviation flag into a sound. It never computes a dose.
 */
class SprayerService : LifecycleService(), SprayerBleClient.Listener {

    private lateinit var ble: SprayerBleClient
    private lateinit var player: AlarmPlayer
    private var wakeLock: PowerManager.WakeLock? = null
    private var settings: SettingsState = Settings.state.value
    private var alarmLooping = false

    // Calibration tables arrive one line per point, then OK; collected here
    // and published as a whole so the UI never shows a half table.
    private val pendingDose = mutableListOf<DosePoint>()
    private val pendingPwm = mutableListOf<PwmPoint>()

    // Every command line gets exactly one OK / BUSY / ERR from the board, in
    // order, since RemoteSprayer answers a line completely before reading the
    // next. So a FIFO of deferreds is all the request/reply matching needs.
    // Everything here runs on the main thread: BLE callbacks are posted to it
    // and lifecycleScope is Main.
    private val pendingReplies = ArrayDeque<CompletableDeferred<Reply>>()

    lateinit var wizard: CalibrationWizard
        private set

    override fun onCreate() {
        super.onCreate()
        Settings.init(this)
        player = AlarmPlayer(this)
        ble = SprayerBleClient(this, this)
        SprayerController.service = this
        wizard = CalibrationWizard(
            scope = lifecycleScope,
            command = { line -> command(line) },
            publish = { w -> SprayerController.publish { it.copy(wizard = w) } },
        )

        lifecycleScope.launch {
            Settings.state.collect { s ->
                settings = s
                player.volume = s.volume
                player.prepare(s.sound)
                if (!s.alarmEnabled && alarmLooping) stopAlarm()
            }
        }
        lifecycleScope.launch {
            SprayerController.state
                .distinctUntilChangedBy { Triple(it.connection, it.status?.requestedLha, it.status?.actualLha) }
                .collect { updateNotification(it) }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        when (intent?.action) {
            ACTION_STOP -> {
                shutdown()
                return START_NOT_STICKY
            }
            else -> {
                if (!goForeground()) {
                    stopSelf()
                    return START_NOT_STICKY
                }
                acquireWakeLock()
                ble.start()
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        SprayerController.service = null
        ble.stop()
        player.release()
        releaseWakeLock()
        SprayerController.publish { SprayerState() }
        super.onDestroy()
    }

    // ------------------------------------------------------------ commands

    fun refresh() {
        send(SprayerProtocol.CMD_CAL_GET)
        send(SprayerProtocol.CMD_CFG_GET)
    }

    fun sendRaw(line: String) = send(line.trim())

    fun setNmea(on: Boolean) = send(SprayerProtocol.cmdTelemetryNmea(on))

    fun setConfig(key: String, value: Long) {
        lifecycleScope.launch {
            val r = command(SprayerProtocol.cmdCfgSet(key, value))
            if (r == Reply.Ok) SprayerController.publish { it.copy(lastMessage = uiText(R.string.msg_saved)) }
            command(SprayerProtocol.CMD_CFG_GET)
        }
    }

    /** Serial menu option 4: change one pump point's measured flow. */
    fun editPwmPointFlow(index: Int, flowMlMin: Int) {
        val point = SprayerController.state.value.pwmPoints.firstOrNull { it.index == index } ?: return
        lifecycleScope.launch {
            var r = command(SprayerProtocol.cmdCalMode(true))
            if (r == Reply.Ok) r = command(SprayerProtocol.cmdCalPwm(index, point.pwm, flowMlMin))
            if (r == Reply.Ok) r = command(SprayerProtocol.CMD_CAL_SAVE)
            command(SprayerProtocol.cmdCalMode(false))
            if (r == Reply.Ok) SprayerController.publish { it.copy(lastMessage = uiText(R.string.msg_saved)) }
            command(SprayerProtocol.CMD_CAL_GET)
        }
    }

    fun clearWizard() {
        SprayerController.publish { it.copy(wizard = null) }
        wizard.cancelQuietly()
    }

    /** Send one line and wait for the board's reply, or a timeout. */
    suspend fun command(line: String): Reply {
        if (!ble.isConnected) return Reply.Error("disconnected")
        val deferred = CompletableDeferred<Reply>()
        pendingReplies.add(deferred)
        if (!ble.sendCommand(line, SprayerProtocol.isProtected(line))) {
            pendingReplies.remove(deferred)
            return Reply.Error("not sent")
        }
        log("> $line")
        // Pairing can take as long as the operator needs to type the code.
        val r = withTimeoutOrNull(if (SprayerProtocol.isProtected(line)) PAIRING_TIMEOUT_MS else COMMAND_TIMEOUT_MS) { deferred.await() }
        if (r == null) pendingReplies.remove(deferred)
        return r ?: Reply.Error("timeout")
    }

    fun previewSound(sound: AlarmSound) {
        alarmLooping = false
        player.play(sound, loop = false)
    }

    fun stopAlarm() {
        alarmLooping = false
        player.stop()
        SprayerController.publish { it.copy(alarmActive = false) }
    }

    private fun send(line: String) {
        if (line.isEmpty()) return
        lifecycleScope.launch { command(line) }
    }

    // ------------------------------------------------------------ BLE events

    override fun onConnectionState(state: ConnectionState, deviceName: String?) {
        SprayerController.publish {
            it.copy(
                connection = state,
                deviceName = deviceName ?: it.deviceName.takeIf { state != ConnectionState.OFF },
                status = if (state == ConnectionState.CONNECTED) it.status else null,
                gps = if (state == ConnectionState.CONNECTED) it.gps else null,
                runSecondsRemaining = null,
                lastMessage = when (state) {
                    ConnectionState.OFF -> uiText(R.string.msg_link_off)
                    ConnectionState.SCANNING -> uiText(R.string.msg_searching)
                    ConnectionState.CONNECTING -> uiText(R.string.msg_connecting)
                    ConnectionState.CONNECTED -> uiText(R.string.msg_connected)
                },
            )
        }
        if (state == ConnectionState.CONNECTED) {
            // The board sends V: on its own once notifications are on; ask
            // for the tables and settings, then switch the periodic lines on.
            refresh()
            send(SprayerProtocol.cmdTelemetryStatus(true))
            send(SprayerProtocol.cmdTelemetryGps(true))
            SprayerController.setNmea(false)   // board-side switch is off after a (re)connect
        } else {
            // The board keeps its own buzzer going; the phone has nothing to
            // base an alarm on without the link. Every command still waiting
            // for a reply fails now, and the wizard learns the board has taken
            // calibration back.
            if (alarmLooping) stopAlarm()
            while (pendingReplies.isNotEmpty()) pendingReplies.poll()?.complete(Reply.Error("disconnected"))
            wizard.onDisconnected()
        }
    }

    override fun onLine(line: String) {
        // The 5 Hz status and 1 Hz GPS lines are on the Status screen; in the
        // console they would push every reply out of view within seconds,
        // unless the operator asked to see them.
        val telemetry = line.startsWith("S:") || line.startsWith("G:")
        if (!telemetry || SprayerController.showTelemetry.value) log("< $line")
        when (val m = SprayerProtocol.parse(line)) {
            is BoardMessage.Version -> SprayerController.publish {
                it.copy(firmwareVersion = m.firmware, protocolVersion = m.protocol)
            }
            is BoardMessage.Status -> onStatus(m)
            is BoardMessage.Gps -> SprayerController.publish { it.copy(gps = m.sample) }
            is BoardMessage.DoseCalPoint -> pendingDose += m.point
            is BoardMessage.PwmCalPoint -> pendingPwm += m.point
            is BoardMessage.ConfigValue -> SprayerController.publish {
                it.copy(config = it.config + (m.key to m.value))
            }
            is BoardMessage.RunCountdown -> {
                SprayerController.publish {
                    it.copy(runSecondsRemaining = if (m.secondsRemaining > 0) m.secondsRemaining else null)
                }
                wizard.onCountdown(m.secondsRemaining)
            }
            BoardMessage.Ok -> {
                commitPendingTables()
                pendingReplies.poll()?.complete(Reply.Ok)
            }
            BoardMessage.Busy -> {
                pendingDose.clear(); pendingPwm.clear()
                SprayerController.publish { it.copy(lastMessage = uiText(R.string.msg_board_busy)) }
                pendingReplies.poll()?.complete(Reply.Busy)
            }
            is BoardMessage.Error -> {
                pendingDose.clear(); pendingPwm.clear()
                SprayerController.publish { it.copy(lastMessage = uiText(R.string.msg_board_refused, m.reason)) }
                pendingReplies.poll()?.complete(Reply.Error(m.reason))
            }
            is BoardMessage.Nmea -> Unit   // already in the log; nothing else to do
            is BoardMessage.Unknown -> Log.w(TAG, "unknown line: $line")
        }
    }

    override fun onError(message: String) {
        // Straight from the BLE stack; not ours to translate.
        SprayerController.publish { it.copy(lastMessage = UiText.Raw(message)) }
    }

    override fun onPairing(active: Boolean) {
        SprayerController.publish {
            it.copy(
                pairing = active,
                lastMessage = if (active) uiText(R.string.msg_pairing) else it.lastMessage,
            )
        }
    }

    private fun onStatus(m: BoardMessage.Status) {
        val s = m.sample
        val now = SystemClock.elapsedRealtime()
        val wantAlarm = s.deviation && settings.alarmEnabled
        if (wantAlarm && !alarmLooping) {
            alarmLooping = true
            player.play(settings.sound, loop = true)
            if (settings.vibrate) vibrate()
        } else if (!wantAlarm && alarmLooping) {
            alarmLooping = false
            player.stop()
        }
        SprayerController.publish {
            it.copy(status = s, statusAt = now, alarmActive = alarmLooping, lastMessage = uiText(R.string.msg_connected))
        }
    }

    private fun commitPendingTables() {
        if (pendingDose.isEmpty() && pendingPwm.isEmpty()) return
        val dose = pendingDose.sortedBy { it.index }
        val pwm = pendingPwm.sortedBy { it.index }
        pendingDose.clear()
        pendingPwm.clear()
        SprayerController.publish {
            it.copy(
                dosePoints = if (dose.isNotEmpty()) dose else it.dosePoints,
                pwmPoints = if (pwm.isNotEmpty()) pwm else it.pwmPoints,
            )
        }
    }

    private fun log(entry: String) {
        SprayerController.publish {
            val next = it.log + entry
            it.copy(log = if (next.size > SprayerState.LOG_LINES) next.takeLast(SprayerState.LOG_LINES) else next)
        }
    }

    // ------------------------------------------------------------ plumbing

    private fun vibrate() {
        val vibrator: Vibrator? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            (getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager)?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        }
        vibrator?.vibrate(VibrationEffect.createOneShot(VIBRATE_MS, VibrationEffect.DEFAULT_AMPLITUDE))
    }

    private fun goForeground(): Boolean {
        return try {
            val type = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            } else {
                0
            }
            ServiceCompat.startForeground(this, NOTIFICATION_ID, buildNotification(SprayerController.state.value), type)
            true
        } catch (e: Exception) {
            // Typically a missing BLUETOOTH_CONNECT permission on Android 14.
            Log.e(TAG, "startForeground failed", e)
            SprayerController.publish { it.copy(lastMessage = uiText(R.string.msg_cannot_start, e.message.orEmpty())) }
            false
        }
    }

    private fun updateNotification(state: SprayerState) {
        if (SprayerController.service !== this) return
        val manager = getSystemService(NOTIFICATION_SERVICE) as android.app.NotificationManager
        try {
            manager.notify(NOTIFICATION_ID, buildNotification(state))
        } catch (e: SecurityException) {
            Log.w(TAG, "notify failed", e)
        }
    }

    private fun buildNotification(state: SprayerState): Notification {
        val openIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val stopIntent = PendingIntent.getService(
            this, 1,
            Intent(this, SprayerService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val s = state.status
        val text = when {
            state.connection != ConnectionState.CONNECTED -> getString(R.string.notification_disconnected)
            s == null -> getString(R.string.notification_connected_no_status)
            else -> getString(
                R.string.notification_connected,
                s.requestedLha,
                s.actualLha?.let { "%.0f".format(it) } ?: "–",
            )
        }
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(text)
            .setContentIntent(openIntent)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setSilent(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .addAction(0, getString(R.string.notification_stop), stopIntent)
            .build()
    }

    private fun acquireWakeLock() {
        if (wakeLock?.isHeld == true) return
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SprayComputer:service").apply {
            setReferenceCounted(false)
            acquire()
        }
    }

    private fun releaseWakeLock() {
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
    }

    private fun shutdown() {
        stopAlarm()
        ble.stop()
        releaseWakeLock()
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    companion object {
        private const val TAG = "SprayerService"
        const val CHANNEL_ID = "sprayer_service"
        const val NOTIFICATION_ID = 1
        const val ACTION_START = "nl.meijworks.spraycomputerld.START"
        const val ACTION_STOP = "nl.meijworks.spraycomputerld.STOP"
        private const val VIBRATE_MS = 300L
        private const val COMMAND_TIMEOUT_MS = 4_000L
        private const val PAIRING_TIMEOUT_MS = 90_000L

        fun start(context: Context) {
            val intent = Intent(context, SprayerService::class.java).setAction(ACTION_START)
            ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            val intent = Intent(context, SprayerService::class.java).setAction(ACTION_STOP)
            context.startService(intent)
        }
    }
}
